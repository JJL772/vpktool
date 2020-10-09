#include "wad.h"

#include <assert.h>
#include <fstream>
#include <ios>
#include <memory.h>
#include <stdlib.h>

using namespace libvpk;

CWADArchive::CWADArchive(wad_settings_t settings)
	: m_IWAD(false), m_PWAD(false), m_onDisk(false), m_settings(settings), m_fileHandle(nullptr), m_error(false)
{
	memset((void*)&this->m_header, 0, sizeof(this->m_header));
}

CWADArchive::~CWADArchive()
{
	if (m_fileHandle)
		fclose(m_fileHandle);
	m_fileHandle = nullptr;
}

CWADArchive* CWADArchive::read(const std::string& path, wad_settings_t settings)
{
	FILE* fs = fopen(path.c_str(), "rb");
	if (!fs)
		return nullptr;

	CWADArchive* archive = new CWADArchive();
	fread((void*)&archive->m_header, sizeof(wad_header_t), 1, fs);
	archive->m_onDiskName = path;
	archive->m_onDisk     = true;
	archive->m_settings   = settings;
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
	archive->m_files.reserve(archive->m_header.entries);

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
		archive_file_t file;
		file.size     = entry.size;
		file.offset   = entry.offset;
		file.name     = tmpnam;
		file.on_disk  = true; // set to true to indicate that we've been read from the disk
		file.internal = wad_internal_file_t();
		archive->m_files.push_back(file);
	}

	if (settings.bKeepFileHandles)
	{
		archive->m_fileHandle = fs;
	}
	else
		fclose(fs);

	return archive;
}

bool CWADArchive::remove_file(const std::string& file)
{
	for (auto it = m_files.begin(); it != m_files.end(); it++)
	{
		if ((*it).name == file)
		{
			m_files.erase(it);
			this->m_dirty = true;
			return true;
		}
	}
	return false;
}

bool CWADArchive::contains(const std::string& file) const
{
	for (const auto& x : m_files)
		if (x.name == file)
			return true;
	return false;
}

bool CWADArchive::write(const std::string& filename)
{
	assert(filename != "" || (m_onDisk && m_onDiskName != ""));
	std::string newnam = filename != "" ? filename : m_onDiskName;
	FILE*	    fs	   = fopen(newnam.c_str(), "wb");
	if (!fs)
		return false;
	m_header.dir_offset = 0;
	m_header.entries    = m_files.size();
	if (m_IWAD)
		memcpy(&m_header.signature, IWADSignature, 4);
	else
		memcpy(&m_header.signature, PWADSignature, 4);
	fwrite((void*)&m_header, sizeof(wad_header_t), 1, fs);

	/* Write out the file data */
	size_t cur_offset = sizeof(wad_header_t);
	if (this->m_dirty)
	{
		for (auto& x : m_files)
		{
			wad_internal_file_t& wad = std::any_cast<wad_internal_file_t&>(x.internal);
			if (wad.onDisk)
			{
				FILE* _fs = fopen(wad.src.c_str(), "rb");
				if (!_fs)
					continue;

				/* Get the file size */
				fseek(_fs, 0, SEEK_END);
				x.size	 = ftell(_fs);
				x.offset = cur_offset;
				fseek(_fs, 0, SEEK_SET);
				/* Read into memory buffer */
				void* pbuf = malloc(x.size);
				fread(pbuf, x.size, 1, _fs);
				/* Write it out onto the disk */
				fwrite(pbuf, x.size, 1, fs);

				fclose(_fs);
				cur_offset += x.size;
			}
		}
	}

	/* Write all entries */
	for (const auto& x : m_files)
	{
		wad_directory_t entry;
		entry.size   = x.size;
		entry.offset = x.offset;
		strncpy(entry.name, x.name.c_str(), 8);
		fwrite((void*)&entry, sizeof(wad_directory_t), 1, fs);
	}

	/* Now go in an update the pointer to the directory */
	fseek(fs, 8, SEEK_SET);
	int _off = cur_offset;
	fwrite(&_off, sizeof(int), 1, fs);

	fclose(fs);
	return true;
}

bool CWADArchive::add_file(const std::string& name, void* pdat, size_t len)
{
	assert(pdat);
	assert(len > 0);
	if (this->contains(name))
		return false;

	this->m_dirty = true;

	archive_file_t	    file;
	wad_internal_file_t internal_file;
	file.name	     = name;
	file.on_disk	     = false;
	file.dirty	     = true;
	internal_file.onDisk = false; // the file is currently in memory waiting to be written to disk
	internal_file.ptr    = malloc(len);
	internal_file.sz     = len;
	memcpy(internal_file.ptr, pdat, len);
	file.internal = internal_file;

	m_files.push_back(file);
	return true;
}

bool CWADArchive::add_file(const std::string& name, const std::string& path)
{
	if (this->contains(name))
		return false;

	this->m_dirty = true;

	archive_file_t	    file;
	wad_internal_file_t internal_file;
	file.name = name;
	internal_file.onDisk =
		true; // the file is currently already on the disk, so it needs to be read to memory and then written to the archive again
	internal_file.src = path;
	file.internal	  = internal_file;

	m_files.push_back(file);
	return true;
}

void* CWADArchive::read_file(const std::string& file, void* buf, size_t& buflen)
{
	assert(buf);
	assert(buflen > 0);

	for (const auto& x : m_files)
	{
		if (x.name == file)
		{
			const wad_internal_file_t& wad = std::any_cast<const wad_internal_file_t&>(x.internal);
			/* If the file is newly added it will be marked "dirty" */
			/* onDisk means that the file is on the disk, so we'll need to read it */
			if (x.dirty && wad.onDisk)
			{
			}
			/* otherwise it's just memory */
			else if (x.dirty && !wad.onDisk)
			{
				size_t num = wad.sz > buflen ? buflen : wad.sz;
				memcpy(buf, wad.ptr, num);
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

bool CWADArchive::extract_file(const std::string& file, std::string tgt)
{
	for (const auto& x : m_files)
	{
		if (x.name == file)
		{
			void*  buffer = malloc(x.size);
			size_t buflen = x.size;
			/* Read file into our buffer */
			if (!this->read_file(file, buffer, buflen))
			{
				free(buffer);
				return false;
			}
			/* Attempt to create a new file stream for the resultant file */
			FILE* fs = fopen(tgt.c_str(), "wb");
			if (!fs)
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
	fprintf(fs, "Archive: %s\n\tNum Files: %u\n\tDirectory offset: %u\n\tType: %s\n", this->m_onDiskName.c_str(), this->m_header.entries,
		this->m_header.dir_offset, this->m_IWAD ? "IWAD" : "PWAD");
}

size_t CWADArchive::file_size(const std::string& file)
{
	for(const auto& x : this->m_files)
	{
		if(x.name == file)
			return x.size;
	}
	return 0;
}

wad_internal_file_t::wad_internal_file_t(wad_internal_file_t&& other) noexcept
{
	this->onDisk = other.onDisk;
	this->src    = other.src;
	this->ptr    = other.ptr;
	this->sz     = other.sz;
	other.sz     = 0;
	other.ptr    = nullptr;
	other.onDisk = false;
	other.src    = "";
}

wad_internal_file_t::wad_internal_file_t(const wad_internal_file_t& other)
{
	this->onDisk = other.onDisk;
	this->src    = other.src;
	this->ptr    = other.ptr;
	this->sz     = other.sz;
}

wad_internal_file_t::wad_internal_file_t() : onDisk(true), ptr(nullptr), sz(0), src("") {}

wad_internal_file_t::~wad_internal_file_t()
{
	if (ptr)
		free(ptr);
	sz  = 0;
	ptr = nullptr;
}
