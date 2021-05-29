#include <iostream>
#include <fstream>
#include <ios>
#include <memory>
#include <cstring>

#include "vpk.hpp"
#include "util.hpp"

using namespace vpklib;

constexpr size_t MAX_TOKEN_STRING = 512;

bool VPK2Archive::read(const void* mem, size_t size) {

	util::ReadContext stream(static_cast<const char*>(mem), size);

	try
	{
		auto header = stream.read<vpk2::Header>();

		if(header.signature != VPK_SIGNATURE || header.version != vpk2::VERSION)
			return false;

		auto fileDataOffset = header.file_data_section_size + sizeof(vpk2::Header);

		// Layer 1: extension
		while(true) {
			char extension[MAX_TOKEN_STRING];
			stream.read_string(extension);

			if(!*extension)
				break;

			// Layer 2: Directory
			while(true) {
				char directory[MAX_TOKEN_STRING];
				stream.read_string(directory);

				if(!*directory)
					break;

				// Layer 3: File name
				while(true) {
					char filename[MAX_TOKEN_STRING];
					stream.read_string(filename);

					if(!*filename)
						break;

					auto dirent = stream.read<vpk2::DirectoryEntry>();

					auto file = std::make_unique<File>();
					file->archive_index = dirent.archive_index;
					file->dirty = false;
					file->length = dirent.entry_length;
					file->preload_size = dirent.preload_bytes;
					file->offset = dirent.entry_offset;
					file->crc = dirent.crc;

					auto key = m_files.size();
					m_files.push_back(file);

					std::string fullName = directory;
					fullName.append("/");
					fullName.append(filename);
					fullName.append(".");
					fullName.append(extension);
					m_handles.insert({fullName, key});
				}
			}
			break;
		}

	}
	catch (std::exception any)
	{
		return false;
	}

}

VPK2Archive* VPK2Archive::read_from_disk(const std::filesystem::path& path) {
	auto archive = new VPK2Archive();

	std::ifstream stream(path);

	if(!stream.good()) {
		delete archive;
		return nullptr;
	}

	stream.seekg(0, std::ios_base::end);
	auto size = stream.tellg();

	stream.seekg(0, std::ios_base::beg);
	auto data = std::make_unique<char*>(new char[size]);
	stream.read(*data, size);

	if(archive->read(*data, size)) {
		return archive;
	}

	delete archive;
	return nullptr;
}

VPK2Archive* VPK2Archive::read_from_mem(const void* mem, size_t size) {
	auto archive = new VPK2Archive();

	if(!archive->read(mem, size)) {
		delete archive;
		return nullptr;
	}

	return archive;
}

VPKFileHandle VPK2Archive::find_file(const std::string& name) {
	// TODO: Force the name to use POSIX style slashes?
	auto it = m_handles.find(name);
	if(it != m_handles.end()) {
		return it->second;
	}
	else {
		return INVALID_HANDLE;
	}
}

size_t VPK2Archive::get_file_size(const std::string& name) {
	return get_file_size(find_file(name));
}

size_t VPK2Archive::get_file_size(VPKFileHandle handle) {
	if(handle == INVALID_HANDLE)
		return 0;

	return 0;
}

size_t VPK2Archive::get_file_preload_size(const std::string& name) {
	return get_file_preload_size(find_file(name));
}

size_t VPK2Archive::get_file_preload_size(VPKFileHandle handle) {
	if(handle == INVALID_HANDLE)
		return 0;

	return 0;
}

UniquePtr<void> VPK2Archive::get_file_preload_data(const std::string& name) {
	return get_file_preload_data(find_file(name));
}

UniquePtr<void> VPK2Archive::get_file_preload_data(VPKFileHandle handle) {
	if(handle == INVALID_HANDLE)
		return vpklib::UniquePtr<void>();

	return vpklib::UniquePtr<void>();
}

std::pair<SizeT, UniquePtr<void>> VPK2Archive::get_file_data(const std::string& name) {
	return get_file_data(find_file(name));
}

std::pair<SizeT, UniquePtr<void>> VPK2Archive::get_file_data(VPKFileHandle handle) {
	if(handle == INVALID_HANDLE)
		return std::pair<SizeT, UniquePtr<void>>();
	return std::pair<SizeT, UniquePtr<void>>();
}

VPK2Search VPK2Archive::get_all_files() {
	return VPK2Search(0, m_files.size(), this);
}
