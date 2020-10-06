#include "pak.h"
#include "util.h"

#include <iostream>
#include <fstream>
#include <cstring>

using namespace libvpk;

libvpk::CPAKArchive::CPAKArchive(libvpk::pak_settings_t settings) 
{
	
}


libvpk::CPAKArchive::~CPAKArchive() 
{
	
}

libvpk::CPAKArchive* libvpk::CPAKArchive::read(const std::string& path, libvpk::pak_settings_t settings) 
{
	FILE* fs = fopen(path.c_str(), "r");
	
	if(!fs) {
		return nullptr;
	}
	
	CPAKArchive* archive = new CPAKArchive(settings);
	pak_header_t hdr;
	fread(&hdr, sizeof(pak_header_t), 1, fs);
	
	if(std::strncmp(hdr.magic, PAKSignature, 4) != 0) {
		delete archive;
		fclose(fs);
		return nullptr;
	}
	archive->m_fileHandle = fs;
	
	size_t entries = hdr.dirsize / sizeof(pak_entry_t);
	archive->m_files.reserve(entries);
	
	for(uint32_t i = 0; i < entries; i++) {
		pak_entry_t ent;
		if(fread(&ent, sizeof(pak_entry_t), 1, fs) != sizeof(pak_entry_t)) {
			/* Someone lied about the number of entries!! */
			fclose(fs);
			delete archive;
			return nullptr;
		}
		
		pak_internal_file_t ifile;
		archive_file_t file;
		file.internal = ifile;
		file.offset = ent.offset;
		file.size = ent.size;
		file.dirty = false;
		
		/* Fix up the file's directory and name and stuff */
		char name[56];
		memcpy(name, ent.filename, 56); // NOTE: Just copying to a temp buffer so crashes dont occur when we have a malformed entry
		name[55] = 0;
		file.name = std::string(name);
		file.dir = libvpk::ExtractDirectory(file.name);
		file.ext = libvpk::ExtractFileExtension(file.name);
		
		archive->m_files.push_back(file);
	}
	
	return archive;
}

bool libvpk::CPAKArchive::add_file(const std::string& name, const std::string& path) 
{
	
}

bool libvpk::CPAKArchive::add_file(const std::string& name, void* data, size_t len) 
{
	
}

void libvpk::CPAKArchive::calc_offsets() 
{
	
}

bool libvpk::CPAKArchive::contains(const std::string& file) const 
{
	
}

void libvpk::CPAKArchive::dump_info(FILE* stream) 
{
	
}

bool libvpk::CPAKArchive::extract_file(const std::string& file, std::string tgt) 
{
	
}

bool libvpk::CPAKArchive::remove_file(const std::string& file) 
{
	
}

bool libvpk::CPAKArchive::write(const std::string& filename) 
{
	
}
