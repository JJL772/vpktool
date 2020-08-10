#include "wad.h"

#include <assert.h>
#include <fstream>
#include <ios>
#include <memory.h>
#include <stdlib.h>

using namespace libvpk;

CWADArchive::CWADArchive(wad_settings_t settings) : m_IWAD(false), m_PWAD(false), m_onDisk(false), m_settings(settings), m_fileHandle(nullptr), m_error(false)
{
	memset((void*)&this->m_header, 0, sizeof(this->m_header));
}

CWADArchive* CWADArchive::read(std::string path, wad_settings_t settings)
{
	FILE* fs = fopen(path.c_str(), "rb");
	if (!fs)
		return nullptr;

	CWADArchive* archive = new CWADArchive();
	fread((void*)&archive->m_header, sizeof(wad_header_t), 1, fs);
	archive->m_onDiskName = path;
	archive->m_onDisk   = true;
	archive->m_settings = settings;
	/* Store file pointer if settings say so */
	if (archive->m_settings.bKeepFileHandles)
		archive->m_fileHandle = fs;
	/* Check the signature */
	if (memcmp(archive->m_header.signature, PWADSignature, 4) == 0)
	{
		archive->m_PWAD = true;
		archive->m_IWAD = false;
	}
	else if (memcmp(archive->m_header.signature, IWADSignature, 4) == 0)
	{
		archive->m_PWAD = false;
		archive->m_IWAD = true;
	}
	else
	{
		/* Not a wad file... */
		delete archive;
		fclose(fs);
		return nullptr;
	}

	/* Read directory */
	fseek(fs, archive->m_header.dir_offset, SEEK_SET);

	/* Read each individual entry */
	for (int i = 0; i < archive->m_header.entries; i++)
	{
		wad_directory_t entry;
		fread((void*)&entry, sizeof(wad_directory_t), 1, fs);
		/* Just to make sure the thing is nul terminated */
		char tmpnam[9];
		memset(tmpnam, 0, 9);
		memcpy(tmpnam, entry.name, 8);
		/* Set the file and push it back in the list */
		archive_file_t file = CWADArchive::create_file();
		file.size	    = entry.size;
		file.offset	    = entry.offset;
		file.name	    = tmpnam;
		file.on_disk	    = true; // set to true to indicate that we've been read from the disk
		archive->m_files.push_back(file);
	}

	return archive;
}

archive_file_t CWADArchive::create_file()
{
	archive_file_t file;
	file.wad = new wad_internal_file_t();
	return file;
}

void CWADArchive::destroy_file(archive_file_t* file)
{
	if (file->wad)
		delete file->wad;
}

bool CWADArchive::remove_file(std::string file)
{
	for (auto it = m_files.begin(); it != m_files.end(); it++)
	{
		if ((*it).name == file)
		{
			m_files.erase(it);
			destroy_file(&(*it));
			return true;
		}
	}
	return false;
}

bool CWADArchive::contains(std::string file) const
{
	for (const auto& x : m_files)
		if (x.name == file)
			return true;
	return false;
}

bool CWADArchive::write(std::string filename)
{
	assert(filename != "" || (m_onDisk && m_onDiskName != ""));
	filename = filename != "" ? filename : m_onDiskName;
	FILE* fs = fopen(filename.c_str(), "wb");
	if (!fs)
		return false;
	/* Directory will be located right after the wad header */
	m_header.dir_offset = sizeof(wad_header_t) - 1;
	m_header.entries    = m_files.size();
	if (m_IWAD)
		memcpy(&m_header.signature, IWADSignature, 4);
	else
		memcpy(&m_header.signature, PWADSignature, 4);
	fwrite((void*)&m_header, sizeof(wad_header_t), 1, fs);

	/* Write all entries */
	for (const auto& x : m_files)
	{
		wad_directory_t entry;
		entry.size   = x.size;
		entry.offset = x.offset;
		strncpy(entry.name, x.name.c_str(), 8);
		fwrite((void*)&entry, sizeof(wad_directory_t), 1, fs);
	}

	/* Write out file data */
	if (this->m_dirty)
	{
	}

	return true;
}

bool CWADArchive::add_file(std::string name, void* pdat, size_t len)
{
	assert(pdat);
	assert(len > 0);
	if (this->contains(name))
		return false;

	archive_file_t file = create_file();
	file.name	    = name;
	file.on_disk	    = false;
	file.dirty	    = true;
	file.wad->onDisk    = false; // the file is currently in memory waiting to be written to disk
	file.wad->dat.ptr   = malloc(len);
	file.wad->dat.sz    = len;
	memcpy(file.wad->dat.ptr, pdat, len);

	m_files.push_back(file);
	return true;
}

bool CWADArchive::add_file(std::string name, std::string path)
{
	if (this->contains(name))
		return false;

	archive_file_t file = create_file();
	file.name	    = name;
	file.wad->onDisk = true; // the file is currently already on the disk, so it needs to be read to memory and then written to the archive again
	file.wad->src	 = path;

	m_files.push_back(file);
	return true;
}

void* CWADArchive::read_file(std::string file, void* buf, size_t& buflen)
{
	assert(buf);
	assert(buflen > 0);

	for (const auto& x : m_files)
	{
		if (x.name == file)
		{
			/* If the file is newly added it will be marked "dirty" */
			/* onDisk means that the file is on the disk, so we'll need to read it */
			if (x.dirty && x.wad->onDisk)
			{

			}
			/* otherwise it's just memory */
			else if(x.dirty && !x.wad->onDisk)
			{
				size_t num = x.wad->dat.sz > buflen ? buflen : x.wad->dat.sz;
				memcpy(buf, x.wad->dat.ptr, num);
				buflen = num;
				return buf;
			}
			/* Otherwise, it's marked as 'on_disk', meaning in the archive still */
			else if (x.on_disk)
			{
				/* Open the file if it's not already open */
				if (!m_fileHandle)
				{
					m_fileHandle = fopen(this->m_onDiskName.c_str(), "rb");
					if (!m_fileHandle)
						return nullptr; // not able to open the file
				}

				/* Seek to offset */
				fseek(m_fileHandle, x.offset, SEEK_SET);
				size_t num = x.size > buflen ? buflen : x.size; // to prevent buffer overruns...
				fread(buf, x.size, 1, m_fileHandle);

				/* Clean up the file handle if we DONT want to keep it */
				if (!m_settings.bKeepFileHandles)
				{
					fclose(m_fileHandle);
					m_fileHandle = nullptr;
				}
				return buf;
			}
		}
	}
	return nullptr;
}

bool CWADArchive::extract_file(std::string file, std::string tgt)
{
	for(const auto& x : m_files)
	{
		if(x.name == file)
		{
			void* buffer = malloc(x.size);
			size_t buflen = x.size;
			/* Read file into our buffer */
			if(!this->read_file(file, buffer, buflen))
			{
				free(buffer);
				return false;
			}
			/* Attempt to create a new file stream for the resultant file */
			FILE* fs = fopen(tgt.c_str(), "wb");
			if(!fs)
			{
				free(buffer);
				return false;
			}
			/* Write to the file */
			fwrite(buffer, x.size, 1, fs);
			fclose(fs);
			free(buffer);
			return true;
		}
	}
	return false;
}

void CWADArchive::dump_info(FILE* fs)
{
	fprintf(fs, "Archive: %s\n\tNum Files: %u\n\tDirectory offset: %u\n\tType: %s\n", this->m_onDiskName.c_str(),
		this->m_header.entries, this->m_header.dir_offset, this->m_IWAD ? "IWAD" : "PWAD");
}
