//===================================================================
// file: vpk1.cpp
// date: 04-05-2020 DD-MM-YYYY
// author: Jeremy L.
// purpose: Implementation of VPK1 archive reading/writing
//===================================================================
#include "vpk1.h"

#include <filesystem>
#include <fstream>
#include <ios>
#include <iosfwd>
#include <iostream>
#include <unordered_map>

using namespace libvpk;

CVPK1Archive::CVPK1Archive() : readonly(false), last_error(EError::NONE), num_archives(0), archive_sizes(nullptr) {}

CVPK1Archive::~CVPK1Archive() {}

CVPK1Archive* CVPK1Archive::read(std::string archive, vpk1_settings_t settings)
{
	CVPK1Archive* pArch = new CVPK1Archive();

	pArch->readonly = settings.readonly;
	pArch->settings = settings;

	std::ifstream fs(archive);

	if (!fs.good())
	{
		pArch->last_error = EError::FILE_NOT_FOUND;
		return pArch;
	}

	fs.read((char*)&pArch->header, sizeof(vpk1_header_t));

	/* Verify the signature is OK */
	if (pArch->header.signature != VPK1Signature)
	{
		pArch->last_error = EError::INVALID_SIG;
		return pArch;
	}

	/* Verify that the version is 1 */
	if (pArch->header.version != VPK1Version)
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
	for (; fs.getline(ext, sizeof(ext), '\0') && ext[0];)
	{
		/* Loop through all directories */
		for (; fs.getline(dir, sizeof(dir), '\0') && dir[0];)
		{
			/* Loop through all files */
			for (; fs.getline(file, sizeof(dir), '\0') && file[0];)
			{
				/* VPK dir entry info */
				vpk1_directory_entry_t dirent;
				fs.read((char*)&dirent, sizeof(dirent));

				/* Basic file entry stuff */
				archive_file_t _file;
				vpk1_file_t internal_file;
				_file.dir	     = dir;
				_file.ext	     = ext;
				_file.name	     = file;
				internal_file.dirent    = dirent;
				internal_file.full_file = _file.dir + "/" + _file.name + "." + _file.ext;

				/* preload bytes follow the directory entry */
				if (internal_file.dirent.preload_bytes > 0)
				{
					/* Only store the preload data if we
					 * really want it */
					if (pArch->settings.keep_preload_data)
					{
						internal_file.preload = malloc(dirent.preload_bytes);
						fs.read((char*)internal_file.preload, dirent.preload_bytes);
					}
					else
					{
						/* ...otherwise, skip the
						 * preload data */
						fs.seekg(dirent.preload_bytes, std::ios_base::cur);
						internal_file.preload = nullptr;
					}
				}

				/* Save the file */
				_file.internal = internal_file;
				pArch->files.push_back(_file);

				/* Update the size of each archive */
				if (internal_file.dirent.archive_index > pArch->num_archives)
				{
					pArch->num_archives  = internal_file.dirent.archive_index + 1;
					pArch->archive_sizes = (size_t*)realloc(pArch->archive_sizes, pArch->num_archives);
					pArch->archive_sizes[internal_file.dirent.archive_index] =
						internal_file.dirent.entry_offset + internal_file.dirent.entry_length;
				}
				/* Compute new max for the archive */
				else
				{
					int    index		    = internal_file.dirent.archive_index;
					size_t sz		    = internal_file.dirent.entry_offset + internal_file.dirent.entry_length;
					pArch->archive_sizes[index] = sz > pArch->archive_sizes[index] ? sz : pArch->archive_sizes[index];
				}


				*file = 0;
			}
			*dir = 0;
		}
		*ext = 0;
	}

	return pArch;
}

void CVPK1Archive::dump_info(FILE* fs)
{
	fprintf(fs,
		"Signature: 0x%X\nVersion: %u\nTotal Size: %u\nNumber of "
		"files: %lu\n",
		header.signature, header.version, header.treesize, files.size());
}

bool CVPK1Archive::remove_file(const std::string& file)
{
	for (auto it = files.begin(); it != files.end(); it++)
	{
		vpk1_file_t vpk1_file = std::any_cast<vpk1_file_t>(it->internal);
		if (vpk1_file.full_file == file)
		{
			files.erase(it);
			return true;
		}
	}
	return false;
}

bool CVPK1Archive::add_file(const std::string&, void* data, size_t sz) {}

void* CVPK1Archive::read_file(const std::string& file, void* buf, size_t& len) {}

bool CVPK1Archive::contains(const std::string& file) const {}

bool CVPK1Archive::add_file(const std::string& name, const std::string& fondisk)
{
	/* First thing to do is get the size of the file */
	std::filesystem::path filepath(fondisk);
	std::filesystem::path destfile(name);
	auto		      filesize = std::filesystem::file_size(filepath);

	/* Next, check if we can just throw this in the dir vpk as preload data
	 */
	if (filesize <= this->settings.max_preload_size)
	{
		archive_file_t	       _file;
		vpk1_file_t internal_file;
		vpk1_directory_entry_t dirent;

		/* Read file data into a buffer */
		void*	      fdat = malloc(filesize);
		std::ifstream fs(fondisk);
		if (!fs.good())
			return false;
		fs.read((char*)fdat, filesize);
		fs.close();

		/* Dirent stuff is mostly 0 */
		dirent.archive_index = 0;
		dirent.entry_length  = 0;
		dirent.entry_offset  = 0;
		dirent.preload_bytes = filesize;
		dirent.terminator    = VPK1Terminator;
		dirent.crc	     = crc32(fdat, filesize);

		internal_file.dirent    = dirent;
		internal_file.preload   = fdat;
		internal_file.written   = false;
		internal_file.dirty     = false; /* Not actually dirty, just not written */
		internal_file.full_file = name;
		_file.ext	     = destfile.extension().string();
		_file.name	     = destfile.filename().string();
		_file.dir	     = destfile.parent_path().string();
		internal_file.full_file = name;

		_file.internal = internal_file;
		this->files.push_back(_file);
		return true;
	}

	/* TODO: Reduce code duplication here */

	for (int i = 0; i < this->num_archives; i++)
	{
		size_t arch_size = this->archive_sizes[i];

		/* Add it to this archive if we've not exceeded the budget */
		if (arch_size + filesize <= this->settings.size_budget)
		{
			archive_file_t	       _file;
			vpk1_file_t internal_file;
			vpk1_directory_entry_t dirent = {};

			dirent.archive_index = i;
			dirent.entry_length  = filesize;
			dirent.entry_offset  = arch_size;
			dirent.terminator    = VPK1Terminator;
			dirent.preload_bytes = 0;
			dirent.crc	     = 0; /* Calculated later */

			internal_file.dirent    = dirent;
			internal_file.dirty     = true;
			internal_file.written   = false;
			_file.dir	     = destfile.parent_path().string();
			_file.name	     = destfile.filename().string();
			_file.ext	     = destfile.extension().string();
			internal_file.preload   = nullptr;
			internal_file.srcfile   = fondisk;
			internal_file.full_file = name;

			_file.internal = internal_file;
			this->files.push_back(_file);

			return true;
		}
	}

	/* Can't fit it in an existing archive...create a new one */
	this->num_archives++;
	this->archive_sizes = (size_t*)realloc(this->archive_sizes, this->num_archives);

	archive_file_t	       _file;
	vpk1_file_t internal_file;
	vpk1_directory_entry_t dirent = {};

	dirent.archive_index = this->num_archives - 1;
	dirent.preload_bytes = 0;
	dirent.terminator    = VPK1Terminator;
	dirent.entry_length  = filesize;
	dirent.entry_offset  = 0;
	dirent.crc	     = 0;

	internal_file.dirent    = dirent;
	internal_file.dirty     = true;
	internal_file.written   = false;
	_file.dir	     = destfile.parent_path().string();
	_file.name	     = destfile.filename().string();
	_file.ext	     = destfile.extension().string();
	internal_file.preload   = nullptr;
	internal_file.srcfile   = fondisk;
	internal_file.full_file = name;

	this->files.push_back(_file);

	return false;
}

bool CVPK1Archive::extract_file(const std::string& file, std::string dest)
{
	/* Find the file to extract */
	for (auto _file : this->files)
	{
		const vpk1_file_t& vpk1_file = std::any_cast<const vpk1_file_t&>(_file.internal);
		if (vpk1_file.full_file == file)
		{
			/* Only read from the paks if the entry length is larger
			 * than 0 */
			if (vpk1_file.dirent.entry_length > 0 && vpk1_file.preload != nullptr)
			{
				/* Open the archive for reading */
				char num[16];
				snprintf(num, sizeof(num), "%03i", vpk1_file.dirent.archive_index);
				printf("%s", (this->base_archive_name + "_" + std::string(num) + ".vpk").c_str());
				std::fstream fs(this->base_archive_name + "_" + std::string(num) + ".vpk", std::ios_base::in | std::ios_base::binary);

				if (!fs.good())
					return false;

				fs.seekg(vpk1_file.dirent.entry_offset, std::ios_base::beg);

				/* Read data and close the stream */
				void* tmpbuf = malloc(vpk1_file.dirent.entry_length);
				fs.read((char*)tmpbuf, vpk1_file.dirent.entry_length);

				fs.close();

				/* Try to open the output */
				fs.open(dest, std::ios_base::out | std::ios_base::binary);

				if (!fs.good())
				{
					fs.close();
					free(tmpbuf);
					return false;
				}

				/* Write the preload bytes first, if applicable
				 */
				if (vpk1_file.dirent.preload_bytes > 0)
					fs.write((char*)vpk1_file.preload, vpk1_file.dirent.preload_bytes);

				/* Write the buffer to the dest */
				fs.write((char*)tmpbuf, vpk1_file.dirent.entry_length);
				fs.flush();

				fs.close();
				return true;
			}
			/* An entry length of 0 means it's all contained within
			 * the preload */
			else
			{
				std::fstream fs(dest, std::ios_base::out | std::ios_base::binary);

				if (!fs.good())
				{
					return false;
				}

				fs.write((char*)vpk1_file.preload, vpk1_file.dirent.preload_bytes);

				fs.flush();
				fs.close();

				return true;
			}
		}
	}
	return false;
}

bool CVPK1Archive::write(const std::string& str)
{
	std::string file;
	if (str == "")
		file = this->base_archive_name + "_dir.vpk";

	/* Create the stream */
	std::fstream fs(file, std::ios_base::binary | std::ios_base::out);

	if (!fs.good())
	{
		fs.close();
		return false;
	}

	/* Write the file header */
	fs.write((char*)&this->header, sizeof(vpk1_header_t));

	/* Basically inverse of the loop used to parse the VPK
	 * First layer is  the extension
	 * Next layer is  the directory
	 * Final layer is the file itself */
	std::string current_ext = "";
	std::string current_dir = "";
	bool	    found	= false;

	std::vector<archive_file_t> _files = this->files;
	bool			    dcond = true, econd = true;
	int			    files_written = 0;

	/* LAYER 1: The file extension */
	/* This is an infinite loop that resets the current extension to ""
	 * after each iteration */
	for (;; current_ext = "", econd = false, dcond = true)
	{
		/* LAYER 2: The directory */
		/* Same as first layer, but for the directory */
		for (; dcond; current_dir = "", dcond = false)
		{
			/* LAYER 3: All files */
			/* Files that do not match the current extension or
			 * directory are skipped */
			/* First iteration sets the current extension and
			 * current directory if either are an empty string */
			/* Files that are written to the disk are removed from
			 * the _files list */
			for (auto it = _files.begin(); it != _files.end(); ++it)
			{
				archive_file_t& _item = *it;
				vpk1_file_t& vpk1_file = std::any_cast<vpk1_file_t&>(it->internal);

				if (vpk1_file.written)
					continue;

				/* Dirty file data ! */
				if (vpk1_file.dirty && !vpk1_file.srcfile.empty())
				{
				}

				/* Set default ext, if not set */
				if (current_ext == "")
				{
					current_ext = _item.ext;
					/* Write out extension and null
					 * terminator */
					fs.write(current_ext.c_str(), current_ext.size());
					fs.put('\0');
				}
				if (_item.ext != current_ext)
					continue;

				/* Set the directory, if not set */
				if (current_dir == "")
				{
					current_dir = _item.dir;
					/* Write out the directory */
					fs.write(current_dir.c_str(), current_dir.size());
					fs.put('\0');
				}
				if (_item.dir != current_dir)
					continue;

				++files_written;

				/* Signal that we've not yet run out of files */
				dcond		   = true;
				vpk1_file.written = true;

				/* Write the file name & null terminator*/
				fs.write(_item.name.c_str(), _item.name.size());
				fs.put('\0');

				/* Ensure that the terminator is ffff */
				vpk1_file.dirent.terminator = 0xFFFF;

				/* Write the directory entry data */
				fs.write((char*)&vpk1_file.dirent, sizeof(vpk1_directory_entry_t));

				if (vpk1_file.dirent.preload_bytes > 0 && vpk1_file.preload == nullptr)
				{
					printf("Error in "
					       "file!\npreload_bytes=%u\n",
					       vpk1_file.dirent.preload_bytes);
				}

				/* Write any preload data we might have */
				if (vpk1_file.dirent.preload_bytes > 0 && vpk1_file.preload)
					fs.write((char*)vpk1_file.preload, vpk1_file.dirent.preload_bytes);

				/* Write the file to the VPK directory */
				// printf("%s/%s.%s\n", _item.directory.c_str(),
				// _item.name.c_str(), _item.extension.c_str());
				/* Remove the file from the list */
				//_files.erase(it);
			}
			/* NULL terminator for the directory entry */
			fs.put('\0');
		}
		/* NULL terminator for the extension */
		fs.put('\0');
		if (_files.size() == files_written)
			break;
	}

	/* Write the header now */

	printf("Write %u files\n", files_written);

	return true;
}
