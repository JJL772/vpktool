// Guts of VPK loader
#include <iostream>
#include <fstream>
#include <ios>
#include <memory>
#include <cstring>

#include "vpk.hpp"

using namespace vpklib;

constexpr size_t MAX_TOKEN_STRING = 512;

namespace vpklib {
	namespace util {

		struct ReadContext
		{
			std::uint64_t pos = 0;
			std::uint64_t size;
			const char* data;

			ReadContext(const char* _data, std::uint64_t _size) :
				data(_data),
				size(_size)
			{
			}

			template<class T>
			T read() {
				const T* dat = reinterpret_cast<const T*>(data + pos);
				pos += sizeof(T);
				return (*dat);
			}

			void read_string(char* buffer) {
				while(data[pos] != 0) {
					if(pos >= size)
						break;
					(*buffer) = data[pos];
					buffer++;
					pos++;
				}
				pos++;
				*buffer = 0;
			}

			size_t read_bytes(char* buffer, size_t num) {
				if(num + pos >= size)
					return 0;

				std::memcpy(buffer, data, num);
				pos += num;
				return num;
			}

			void set_pos(std::uint64_t pos) {
				if(pos >= size)
					throw std::out_of_range("set_pos called with value >= size");
				pos = pos;
			}

			auto get_pos() {
				return pos;
			}

			void seek(std::uint64_t forward) {
				if(pos + forward >= size)
					throw std::out_of_range("seek called with value that causes overflow");
				pos += forward;
			}

			void seekr(std::uint64_t back) {
				if (pos - back < 0)
					throw std::out_of_range("seekr called with data that causes a underflow");
				pos -= back;
			}

		};

	}
}

//---------------------------------------------------------------------------//

vpk_archive::~vpk_archive() {
	for(int i = 0; i < m_maxPakIndex; i++) {
		if(m_fileHandles[i]) {
			fclose(m_fileHandles[i]);
		}
	}
}

// Read VPK from memory
bool vpk_archive::read(const void* mem, size_t size) {

	util::ReadContext stream(static_cast<const char*>(mem), size);

	// Total size of file data contained within the _dir vpk
	size_t dirFileDataSize = 0;

	try
	{
		auto header = stream.read<vpk::Header>();

		if(header.signature != VPK_SIGNATURE || (header.version != 2 && header.version != 1)) {
			printf("Signature did not match %X    ver %d\n", header.signature, header.version);
			return false;
		}
		
		this->version = header.version;
		
		size_t headerSize = sizeof(vpk::Header);
		if(this->version == 2) {
			headerSize += sizeof(vpk2::HeaderExt);
		}
		
		auto sectionMD5Size = 0ull;
		auto signatureSectionSize = 0ull;
		
		if(header.version == 2) {
			auto headerext = stream.read<vpk2::HeaderExt>();
			sectionMD5Size = headerext.archive_md5_section_size;
			signatureSectionSize = headerext.signature_section_size;
		}

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

				// If the directory is a single space, we actually don't have a directory! Indicate that.
				if (*directory == ' ' && !*(directory+1))
					*directory = 0;
				else
					m_dirs.insert(directory);

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
					
					// If archive_index is 0x7FFF, entry offset is relative to sizeof(Header) + treeLength
					// and the data is in _dir.vpk
					if(dirent.archive_index == 0x7FFF) {
						file->offset = dirent.entry_offset + headerSize + header.tree_size;
						dirFileDataSize += file->length;
					}
					else {
						// Wrap this in an else so we don't mistakenly create 0x7FFF handles
						m_maxPakIndex = file->archive_index > m_maxPakIndex ? file->archive_index : m_maxPakIndex;
					}

					// Read any preload data we might have
					if(dirent.preload_bytes > 0) {
						file->preload_data = std::make_unique<byte[]>(dirent.preload_bytes);
						stream.read_bytes(file->preload_data.get(), dirent.preload_bytes);
					}

					auto key = m_files.size();
					m_files.push_back(std::move(file));

					std::string fullName = directory;
					if (*directory)
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
		
		// skip the file data stored in this VPK so we can read actually useful stuff
		if(dirFileDataSize)
		stream.set_pos(dirFileDataSize);
		
		// Step 2: Post-dir tree and file data structures
		
		// Read the archive md5 sections 
		if(version == 2) {
			for(size_t i = 0; i < (sectionMD5Size / sizeof(vpk2::ArchiveMD5SectionEntry)); i++) {
				m_archiveSectionEntries.push_back(stream.read<vpk2::ArchiveMD5SectionEntry>());
			}
		}
		
		// Read single OtherMD5Section
		if(version == 2) {
			m_otherMD5Section = stream.read<vpk2::OtherMD5Section>();
		}
		
		// Read signature section 
		if(signatureSectionSize > 0 && version == 2) {
			auto pubKeySize = stream.read<uint32_t>();
			m_signatureSection.pubkey_size = pubKeySize;
			m_signatureSection.pubkey = std::make_unique<char[]>(pubKeySize);
			stream.read_bytes(m_signatureSection.pubkey.get(), pubKeySize);
		
			auto sigSize = stream.read<uint32_t>();
			m_signatureSection.signature_size = sigSize;
			m_signatureSection.signature = std::make_unique<char[]>(sigSize);
			stream.read_bytes(m_signatureSection.signature.get(), sigSize);
		}
	}
	catch (std::exception& any)
	{
		return false;
	}
	
	m_fileHandles = std::make_unique<FILE*[]>(m_maxPakIndex+1);
	return true;

}

vpk_archive* vpk_archive::read_from_disk(const std::filesystem::path& path) {
	auto basename = path.string();
	auto dirSubStr = basename.find("_dir.vpk");
	if (dirSubStr == std::string::npos) {
		return nullptr;
	}
	basename.erase(dirSubStr, basename.size());

	auto archive = new vpk_archive();
	archive->m_baseArchiveName = basename;
	archive->m_dirHandle = fopen(path.string().c_str(), "r");

	if(!archive->m_dirHandle) {
		delete archive;
		return nullptr;
	}

	fseek(archive->m_dirHandle, 0, SEEK_END);
	auto size = ftell(archive->m_dirHandle);

	fseek(archive->m_dirHandle, 0, SEEK_SET);
	auto data = std::make_unique<char*>(new char[size]);
	fread(*data.get(), size, 1, archive->m_dirHandle);

	if(archive->read(*data, size)) {
		return archive;
	}

	delete archive;
	return nullptr;
}

// Find file by name in the archive
vpk_file_handle vpk_archive::find_file(const std::string& name) {
	// TODO: Force the name to use POSIX style slashes?
	auto it = m_handles.find(name);
	if(it != m_handles.end()) {
		return it->second;
	}
	else {
		return INVALID_HANDLE;
	}
}

size_t vpk_archive::get_file_size(const std::string& name) {
	return get_file_size(find_file(name));
}

size_t vpk_archive::get_file_size(vpk_file_handle handle) {
	if(handle == INVALID_HANDLE)
		return 0;
	const auto& file = m_files[handle];
	return file->preload_size + file->length;
}

size_t vpk_archive::get_file_preload_size(const std::string& name) {
	return get_file_preload_size(find_file(name));
}

size_t vpk_archive::get_file_preload_size(vpk_file_handle handle) {
	if(handle == INVALID_HANDLE)
		return 0;
	return m_files[handle]->preload_size;
}

std::tuple<void*, std::size_t>  vpk_archive::get_file_preload_data(const std::string& name) {
	return get_file_preload_data(find_file(name));
}

std::tuple<void*, std::size_t> vpk_archive::get_file_preload_data(vpk_file_handle handle) {
	if(handle == INVALID_HANDLE)
		return std::make_tuple(nullptr, 0);

	const auto &file = m_files[handle];

	if(file->preload_size == 0)
		return std::make_tuple(nullptr, 0);

	auto data = static_cast<char*>(malloc(file->preload_size));
	std::memcpy(data, file->preload_data.get(), file->preload_size);
	return std::make_tuple(data, file->preload_size);
}

std::tuple<void*, std::size_t> vpk_archive::get_file_data(const std::string& name) {
	return get_file_data(find_file(name));
}

// Return a pointer to the file data, and the size of that data
std::tuple<void*, std::size_t> vpk_archive::get_file_data(vpk_file_handle handle) {
	if(handle == INVALID_HANDLE)
		return std::make_tuple(nullptr,0);

	const auto& file = m_files[handle];
	const auto fileSize = static_cast<std::size_t>(get_file_size(handle));
	const auto preloadSize = get_file_preload_size(handle);
	const auto totalSize = preloadSize + file->length;
	auto data = static_cast<char*>(malloc(fileSize));

	// If handle is not open already, open it
	if(!(m_fileHandles)[file->archive_index] && file->archive_index != 0x7FFF) {
		char num[16] = {};
		snprintf(num, sizeof(num), "_%03d.vpk", file->archive_index);
		auto apath = m_baseArchiveName + num;
		m_fileHandles[file->archive_index] = fopen(apath.c_str(), "r");
	}

	// Handle the case where the data is in the _dir PAK
	FILE* archHandle = nullptr;
	if(file->archive_index == 0x7FFF) { 
		archHandle = m_dirHandle;
	}
	else {
		archHandle = m_fileHandles[file->archive_index];
	}

	if(!archHandle) {
		return std::make_tuple(nullptr, 0);
	}

	// Read preload data into the buffer
	get_file_preload_data(handle, data, preloadSize);

	// Now run a fread to grab the rest of the data
	fseek(archHandle, file->offset, SEEK_SET);
	fread(data + preloadSize, file->length, 1, archHandle);

	return std::make_tuple(data, totalSize);
}

vpk_search vpk_archive::get_all_files() {
	return vpk_search(0, m_files.size(), this);
}

std::string vpk_archive::get_file_name(vpk_file_handle handle) {
	if(handle == INVALID_HANDLE || handle >= m_fileNames.size())
		return "";
	return m_fileNames[handle];
}

vpk_search vpk_archive::find_in_directory(const std::string& path) {
	std::size_t begin = -1, end = 0;
	for(std::size_t i = 0; i < m_fileNames.size(); i++) {
		if(m_fileNames[i].starts_with(path)) {
			if(begin == -1)
				begin = i;
		} else if(begin != -1) {
			end = i;
			return vpk_search(begin, end, this);
		}
	}
	return vpk_search(0, 0, this);
}

std::size_t vpk_archive::get_file_preload_data(vpk_file_handle handle, void* buffer, std::size_t bufferSize) {
	if(handle == INVALID_HANDLE)
		return 0;

	const auto &file = m_files[handle];
	const auto bytesToCopy = file->preload_size > bufferSize ? bufferSize : file->preload_size;
	if (bytesToCopy)
		std::memcpy(buffer, file->preload_data.get(), bytesToCopy);
	return bytesToCopy;
}

std::size_t vpk_archive::get_file_preload_data(const std::string& name, void* buffer, std::size_t bufferSize) {
	return get_file_preload_data(find_file(name), buffer, bufferSize);
}

size_t vpk_archive::get_pubkey_size() {
	return m_signatureSection.pubkey_size;
}

void* vpk_archive::get_pubkey() {
	return m_signatureSection.pubkey.get();
}

size_t vpk_archive::get_pubkey(void* buffer, size_t bufSize) {
	size_t copied = 0;
	if(m_signatureSection.pubkey_size) {
		copied = m_signatureSection.pubkey_size > bufSize ? bufSize : m_signatureSection.pubkey_size;
		std::memcpy(buffer, m_signatureSection.pubkey.get(), copied);
	}
	return copied;
}

size_t vpk_archive::get_signature_size() {
	return m_signatureSection.signature_size;
}

void* vpk_archive::get_signature() {
	return m_signatureSection.signature.get();
}

size_t vpk_archive::get_signature(void* buffer, size_t bufSize) {
	size_t copied = 0;
	if(m_signatureSection.signature_size) {
		copied = m_signatureSection.signature_size > bufSize ? bufSize : m_signatureSection.signature_size;
		std::memcpy(buffer, m_signatureSection.signature.get(), copied);
	}
	return copied;
}

std::uint16_t vpk_archive::get_file_archive_index(const std::string& name) {
	return get_file_archive_index(find_file(name));
}

std::uint16_t vpk_archive::get_file_archive_index(vpk_file_handle handle) {
	if(handle == INVALID_HANDLE)
		return 0;
	const auto& file = m_files[handle];
	return file->archive_index;
}
		
std::uint32_t vpk_archive::get_file_crc32(const std::string& name) {
	return get_file_crc32(find_file(name));
}

std::uint32_t vpk_archive::get_file_crc32(vpk_file_handle handle) {
	if(handle == INVALID_HANDLE)
		return 0;
	const auto& file = m_files[handle];
	return file->crc;
}

const std::set<std::string>& vpk_archive::get_directories() const {
	return m_dirs;
}