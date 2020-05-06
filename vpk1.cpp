 //===================================================================
 // file: vpk1.cpp
 // date: 04-05-2020 DD-MM-YYYY
 // author: Jeremy L.
 // purpose: Implementation of VPK1 archive reading/writing
 //=================================================================== 
#include "vpk1.h"

#include <fstream>
#include <ios>
#include <iosfwd>
#include <iostream>

bool IsVPK1(std::string file)
{
	std::ifstream fs(file);

	if(!fs.good()) return false;

	vpk1_header_t hdr;
	fs.read((char*)&hdr, sizeof(hdr));

	if(hdr.signature == VPK1Signature && hdr.version == VPK1Version)
	{
		fs.close();
		return true;
	}

	fs.close();

	return false;
}

CVPK1Archive::CVPK1Archive() :
	readonly(false),
	last_error(EError::NONE)
{

}

CVPK1Archive::~CVPK1Archive()
{
	
}

CVPK1Archive* CVPK1Archive::ReadArchive(std::string archive, bool readonly)
{
	CVPK1Archive* pArch = new CVPK1Archive();

	pArch->readonly = readonly;

	std::ifstream fs(archive);

	if(!fs.good()) 
	{
		pArch->last_error = EError::FILE_NOT_FOUND;
		return pArch;
	}

	fs.read((char*)&pArch->header, sizeof(vpk1_header_t));

	/* Verify the signature is OK */
	if(pArch->header.signature != VPK1Signature)
	{
		pArch->last_error = EError::INVALID_SIG;
		return pArch;
	}

	/* Verify that the version is 1 */
	if(pArch->header.version != VPK1Version)
	{
		pArch->last_error = EError::WRONG_VERSION;
		return pArch;
	}

	/* Try to get the base archive name based on what we've been passed
	 * for example, the base name would be pak001 in pak001_dir.vpk 
	 * Should just be able to replace the _dir.vpk with "" */
	pArch->base_archive_name = archive.replace(archive.find("_dir.vpk"), archive.size(), "");
	

	/* Bulk of the data processing is here.
	 * First layer of the loop will go through the file extensions
	 * Next layer will read the directories
	 * Final layer will read the individual file names and file info */
	char ext[32];
	char dir[300]; /* Keep this above 260 (the max path for windurs */
	char file[300];
	
	/* Loop through all file extensions */
	for(; fs.getline(ext, sizeof(ext), '\0') && ext[0]; )
	{
		/* Loop through all directories */
		for(; fs.getline(dir, sizeof(dir), '\0') && dir[0]; )
		{
			/* Loop through all files */
			for(; fs.getline(file, sizeof(dir), '\0') && file[0]; )
			{
				/* VPK dir entry info */
				vpk1_directory_entry_t dirent;
				fs.read((char*)&dirent, sizeof(dirent));

				/* Basic file entry stuff */
				vpk1_file_t _file;
				_file.directory = dir;
				_file.extension = ext;
				_file.name = file;
				_file.dirent = dirent;
				_file.full_file = _file.directory + "/" + _file.name + "." + _file.extension;
				pArch->files.push_back(_file);
			
				/* preload bytes follow the directory entry */
				if(_file.dirent.preload_bytes > 0)
				{
					_file.preload = malloc(dirent.preload_bytes);
					fs.read((char*)_file.preload, dirent.preload_bytes);
				}

				*file = 0;
			}
			*dir = 0;
		}
		*ext = 0;
	}

	return pArch;
}

void CVPK1Archive::DumpInfo(FILE* fs)
{
	fprintf(fs, "Signature: 0x%X\nVersion: %u\nTotal Size: %u\nNumber of files: %lu\n",
			header.signature, header.version, header.treesize, files.size());
}

bool CVPK1Archive::RemoveFile(std::string file)
{
	return false;
}

bool CVPK1Archive::AddFile(std::string name, std::string fondisk)
{
	return false;
}

const std::vector<vpk1_file_t>& CVPK1Archive::GetFiles() const
{
	return this->files;
}

bool CVPK1Archive::ExtractFile(std::string file, std::string dest)
{
	/* Find the file to extract */
	for(auto _file : this->files)
	{
		if(_file.full_file == file)
		{
			/* Only read from the paks if the entry length is larger than 0 */
			if(_file.dirent.entry_length > 0)
			{
				/* Open the archive for reading */
				char num[16];
				snprintf(num, sizeof(num), "%03i", _file.dirent.archive_index);
				printf("%s", (this->base_archive_name + "_" + std::string(num) + ".vpk").c_str());
				std::fstream fs(this->base_archive_name + "_" + std::string(num) + ".vpk", 
						std::ios_base::in | std::ios_base::binary);
			
				if(!fs.good()) return false;

				fs.seekg(_file.dirent.entry_offset, std::ios_base::beg);
				
				/* Read data and close the stream */
				void* tmpbuf = malloc(_file.dirent.entry_length);
				fs.read((char*)tmpbuf, _file.dirent.entry_length);

				fs.close();

				/* Try to open the output */
				fs.open(dest, std::ios_base::out | std::ios_base::binary);

				if(!fs.good())
				{
					fs.close();
					free(tmpbuf);
					return false;
				}

				/* Write the preload bytes first, if applicable */
				if(_file.dirent.preload_bytes > 0)
					fs.write((char*)_file.preload, _file.dirent.preload_bytes);

				/* Write the buffer to the dest */
				fs.write((char*)tmpbuf, _file.dirent.entry_length);
				fs.flush();

				fs.close();
				return true;
			}
			/* An entry length of 0 means it's all contained within the preload */
			else
			{
				std::fstream fs(dest, std::ios_base::out | std::ios_base::binary);

				if(!fs.good())
				{
					return false;
				}

				fs.write((char*)_file.preload, _file.dirent.preload_bytes);

				fs.flush();
				fs.close();

				return true;
			}
		}
		
	}
	return false;
}
