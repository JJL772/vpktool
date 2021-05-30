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

		if(header.signature != VPK_SIGNATURE || header.version != vpk2::VERSION) {
			printf("Signature did not match %X    ver %d\n", header.signature, header.version);
			return false;
		}

		auto fileDataOffset = header.file_data_section_size + sizeof(vpk2::Header);

		// Layer 1: extension
		while(true) {
			char extension[MAX_TOKEN_STRING];
			*extension = 0;
			stream.read_string(extension);

			if(!*extension)
				break;

			// Layer 2: Directory
			while(true) {
				char directory[MAX_TOKEN_STRING];
				*directory = 0;
				stream.read_string(directory);

				if(!*directory)
					break;

				// Layer 3: File name
				while(true) {
					char filename[MAX_TOKEN_STRING];
					*filename = 0;
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

					m_maxPakIndex = file->archive_index > m_maxPakIndex ? file->archive_index : m_maxPakIndex;


					// Read any preload data we might have
					if(dirent.preload_bytes > 0) {
						file->preload_data = std::make_unique<byte[]>(dirent.preload_bytes);
						stream.read_bytes(file->preload_data.get(), dirent.preload_bytes);
					}

					auto key = m_files.size();
					m_files.push_back(std::move(file));

					std::string fullName = directory;
					fullName.append("/");
					fullName.append(filename);
					fullName.append(".");
					fullName.append(extension);
					m_handles.insert({fullName, key});

					// Add to our shit dict
					m_fileNames.push_back(fullName);
				}
			}
		}

	}
	catch (std::exception& any)
	{
		return false;
	}

	m_fileHandles = std::make_unique<FILE*[]>(m_maxPakIndex);

	return true;

}

VPK2Archive* VPK2Archive::read_from_disk(const std::filesystem::path& path) {
	auto archive = new VPK2Archive();

	auto basename = path.string();
	basename.erase(basename.find_last_of("_dir.vpk"), basename.size());
	archive->m_baseArchiveName = basename;

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
	const auto& file = m_files[handle];
	return file->preload_size + file->length;
}

size_t VPK2Archive::get_file_preload_size(const std::string& name) {
	return get_file_preload_size(find_file(name));
}

size_t VPK2Archive::get_file_preload_size(VPKFileHandle handle) {
	if(handle == INVALID_HANDLE)
		return 0;
	return m_files[handle]->preload_size;
}

UniquePtr<char[]> VPK2Archive::get_file_preload_data(const std::string& name) {
	return get_file_preload_data(find_file(name));
}

UniquePtr<char[]> VPK2Archive::get_file_preload_data(VPKFileHandle handle) {
	if(handle == INVALID_HANDLE)
		return vpklib::UniquePtr<char[]>();

	const auto &file = m_files[handle];

	if(file->preload_size == 0)
		return UniquePtr<char[]>();

	auto data = std::make_unique<char[]>(file->preload_size);
	std::memcpy(data.get(), file->preload_data.get(), file->preload_size);
	return std::move(data);
}

std::pair<SizeT, UniquePtr<char[]>> VPK2Archive::get_file_data(const std::string& name) {
	return get_file_data(find_file(name));
}

std::pair<SizeT, UniquePtr<char[]>> VPK2Archive::get_file_data(VPKFileHandle handle) {
	if(handle == INVALID_HANDLE)
		return std::pair<SizeT, UniquePtr<char[]>>();

	const auto& file = m_files[handle];
	const auto fileSize = (SizeT)get_file_size(handle);
	const auto preloadSize = get_file_preload_size(handle);
	const auto totalSize = preloadSize + file->length;
	UniquePtr<char[]> data = std::make_unique<char[]>(fileSize);

	// If handle is not open already, open it
	if(!(m_fileHandles)[file->archive_index]) {
		char num[16];
		snprintf(num, sizeof(num), "_%3d.vpk", file->archive_index);
		auto apath = m_baseArchiveName + num;
		m_fileHandles[file->archive_index] = fopen(apath.c_str(), "r");

		if(m_fileHandles[file->archive_index]) {
			printf("Opened archive %s\n", apath.c_str());
		}
		else {
			printf("Could not open %s\n", apath.c_str());
		}
	}

	auto archHandle = m_fileHandles[file->archive_index];

	if(!archHandle) {
		return std::pair<SizeT, UniquePtr<char[]>>();
	}

	// Read preload data into the buffer
	get_file_preload_data(handle, data.get(), preloadSize);

	// Now run a fread to grab the rest of the data
	fseek(archHandle, file->offset, SEEK_SET);
	fread(data.get() + preloadSize, file->length, 1, archHandle);

	return {totalSize, std::move(data)};
}

VPK2Search VPK2Archive::get_all_files() {
	return VPK2Search(0, m_files.size(), this);
}

std::string VPK2Archive::get_file_name(VPKFileHandle handle) {
	if(handle == INVALID_HANDLE || handle >= m_fileNames.size())
		return "";
	return m_fileNames[handle];
}

VPK2Search VPK2Archive::find_in_directory(const std::string& path) {

	std::size_t begin = -1, end = 0;
	for(std::size_t i = 0; i < m_fileNames.size(); i++) {
		if(m_fileNames[i].starts_with(path)) {
			if(begin == -1)
				begin = i;
		} else if(begin != -1) {
			end = i;
			return VPK2Search(begin, end, this);
		}
	}
	return VPK2Search(0, 0, this);
}

std::size_t VPK2Archive::get_file_preload_data(VPKFileHandle handle, void* buffer, std::size_t bufferSize) {
	if(handle == INVALID_HANDLE)
		return 0;

	const auto &file = m_files[handle];
	const auto bytesToCopy = file->preload_size > bufferSize ? bufferSize : file->preload_size;
	std::memcpy(buffer, file->preload_data.get(), bytesToCopy);
	return bytesToCopy;
}

std::size_t VPK2Archive::get_file_preload_data(const std::string& name, void* buffer, std::size_t bufferSize) {
	return get_file_preload_data(find_file(name), buffer, bufferSize);
}
