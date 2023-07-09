#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include <tuple>
#include <cstring>

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

namespace vpklib
{
	constexpr std::uint32_t VPK_SIGNATURE = 0x55AA1234;
	constexpr std::uint16_t DIRECTORY_TERMINATOR = 0xFFFF;

	using md5_t = char[16];
	using byte = char;

	using vpk_file_handle = std::uint64_t;

	constexpr vpk_file_handle INVALID_HANDLE = ~0ull;

#pragma pack(1)

	// vpk1 specific definitions
	namespace vpk1
	{
		constexpr std::uint32_t VERSION = 1;

		struct Header
		{
			std::uint32_t signature;
			std::uint32_t version;
			std::uint32_t tree_size;
		};

		struct DirectoryEntry
		{
			std::uint32_t crc;
			std::uint16_t preload_bytes;
			std::uint16_t archive_index;
			std::uint32_t entry_offset;
			std::uint32_t entry_length;
		};
	}

	// vpk2 specific definitions
	namespace vpk
	{

		// VPK1/2 combined header
		struct Header
		{
			std::uint32_t signature;
			std::uint32_t version;

			std::uint32_t tree_size;
		};
	}
	
	namespace vpk2 
	{
		
		// Extended VPK2 header
		struct HeaderExt
		{
			std::uint32_t file_data_section_size;
			std::uint32_t archive_md5_section_size;
			std::uint32_t other_md5_section_size;
			std::uint32_t signature_section_size;
		};

		struct DirectoryEntry
		{
			std::uint32_t crc;
			std::uint16_t preload_bytes;
			std::uint16_t archive_index;
			std::uint32_t entry_offset;
			std::uint32_t entry_length;
			std::uint16_t terminator;
		};

		struct ArchiveMD5SectionEntry
		{
			std::uint32_t archive_index;
			std::uint32_t start_offset;
			std::uint32_t count;
			md5_t		  checksum;
		};

		struct OtherMD5Section
		{
			md5_t tree_checksum;
			md5_t archive_md5_section_checksum;
			char unknown[16];
		};

#pragma pack()

		// Not to be read from the disk
		struct SignatureSection
		{
			std::uint32_t pubkey_size;
			std::unique_ptr<byte[]> pubkey;

			std::uint32_t signature_size;
			std::unique_ptr<byte[]> signature;
		};
	}

	std::uint32_t get_vpk_version(const std::filesystem::path &path);
	std::uint32_t get_vpk_version(const void* mem);


	class vpk_archive
	{
	private:
		friend class vpk_search;
		
		std::uint32_t version = 2;

		struct File
		{
			std::int32_t archive_index = 0;

			std::uint32_t crc = 0;

			std::uint16_t preload_size = 0;
			std::unique_ptr<byte[]> preload_data;

			std::uint32_t offset = 0;
			std::uint32_t length = 0;

			bool dirty = false;
		};

		bool m_dirty = false;
		std::string m_baseArchiveName;

		std::unordered_map<std::string, vpk_file_handle> m_handles;
		std::vector<std::string> m_fileNames; // TODO: Make this less garbage
		std::vector<std::unique_ptr<File>> m_files;

		FILE* m_dirHandle;
		std::unique_ptr<FILE*[]> m_fileHandles; // List of all open file handles to the individual archives
		std::uint16_t m_maxPakIndex = 0;

		std::vector<vpk2::ArchiveMD5SectionEntry> m_archiveSectionEntries;
		vpk2::OtherMD5Section m_otherMD5Section;
		vpk2::SignatureSection m_signatureSection;

	private:
		bool read(const void* mem, size_t size);

	public:
		~vpk_archive();
	
		/**
		 * @brief Reads the file from disk 
		 * @param path 
		 * @return VPK2Archive* 
		 */
		static vpk_archive* read_from_disk(const std::filesystem::path& path);

		/**
		 * @brief Returns the version of the archive (1 or 2)
		 * @return int Version
		 */
		int get_version() const { return version; };

		/**
		 * @brief Finds a single file in the archive 
		 * @param name Name of the file to look for 
		 * @return VPKFileHandle 
		 */
		vpk_file_handle find_file(const std::string& name);

		/**
		 * @brief Returns the base archive name
		 * ex: myarchive in myarchive_dir.vpk
		 * @return const std::string& 
		 */
		const std::string& base_archive_name() const { return m_baseArchiveName; };

		/**
		 * @brief Returns the total size of the specified file in bytes
		 * @param name Path to file or it's handle
		 * @return size_t Size in bytes of the file, including any preload data
		 */
		size_t get_file_size(const std::string& name);
		size_t get_file_size(vpk_file_handle handle);

		/**
		 * @brief Returns the size of the preload data associated with the specified file 
		 * @param file The file handle or path to the file 
		 * @return size_t Size of the preload data in bytes
		 */
		size_t get_file_preload_size(const std::string& name);
		size_t get_file_preload_size(vpk_file_handle handle);

		/**
		 * @brief Returns the file preload data associated with the file
		 * You must use `free` to free the pointer returned by this function.
		 * @param name Path to the file or a handle 
		 * @return std::tuple<void*, std::size_t> Tuple containing pointer to data and size of data. Pointer must be freed by caller.
		 */
		std::tuple<void*, std::size_t> get_file_preload_data(const std::string& name);
		std::tuple<void*, std::size_t> get_file_preload_data(vpk_file_handle handle);
		
		/**
		 * @brief Copies file preload data into the specified buffer
		 * @param handle Handle or path to file
		 * @param buffer Buffer to copy into
		 * @param bufferSize Size of the buffer to copy into. Used to prevent overruns
		 * @return std::size_t Number of bytes copied
		 */
		std::size_t get_file_preload_data(vpk_file_handle handle, void* buffer, std::size_t bufferSize);
		std::size_t get_file_preload_data(const std::string& name, void* buffer, std::size_t bufferSize);

		/**
		 * @brief Returns a unique ptr to the file data.
		 * If there is preload data associated with the file, it is concatenated with the actual data.
		 * Returned void* ptr must be freed using `free` by the caller.
		 * @param name Path to file or handle of file
		 * @return std::tuple<void*,std::size_t> data Tuple containing the data and size of the data. Must be freed by caller.
		 */
		std::tuple<void*, std::size_t> get_file_data(const std::string& name);
		std::tuple<void*, std::size_t> get_file_data(vpk_file_handle handle);
		
		/**
		 * @brief Copies the file's data into the specified buffer 
		 * 
		 * @param handle Handle or path to the file
		 * @param buffer Target buffer
		 * @param bufferSize Size of the target buffer
		 * @return size_t Number of bytes copied
		 */
		size_t get_file_data(vpk_file_handle handle, void* buffer, size_t bufferSize);
		size_t get_file_data(const std::string& name, void* buffer, size_t bufferSize);

		/**
		 * @brief Returns the number of files in this archive 
		 * @return size_t 
		 */
		size_t get_file_count() const { return m_fileNames.size(); };

		/**
		 * @brief Returns a generalized search that encompasses all files in the archive 
		 * @return VPK2Search 
		 */
		class vpk_search get_all_files();

		/**
		 * @brief Returns the file name for the handle 
		 * @param handle 
		 * @return std::string 
		 */
		std::string get_file_name(vpk_file_handle handle);

		/**
		 * @brief Finds all files in a directory
		 * @param path Directory path
		 * @return VPK2Search 
		 */
		vpk_search find_in_directory(const std::string& path);

		/**
		 * @brief Returns the size of the public key
		 * @return size_t 
		 */
		size_t get_pubkey_size();
		
		/**
		 * @brief Returns a buffer that contains the public key 
		 * @return 
		 */
		void* get_pubkey();
		size_t get_pubkey(void* buffer, size_t bufSize);

		/**
		 * @brief Returns the signature size 
		 * @return size_t 
		 */
		size_t get_signature_size();
		
		/**
		 * @brief Returns a buffer that contains the signature 
		 * @return std::unique_ptr<char[]> 
		 */
		void* get_signature();
		size_t get_signature(void* buffer, size_t bufSize);
		
		/**
		 * @brief Returns the archive index the file is in
		 * @param name Path or handle to file
		 * @return uint16_t Index
		 */
		std::uint16_t get_file_archive_index(const std::string& name);
		std::uint16_t get_file_archive_index(vpk_file_handle handle);
		
		/**
		 * @brief Returns the file's CRC. NOTE: This returns the reported CRC 
		 * @param name Name or handle to file 
		 * @return std::uint32_t CRC32
		 */
		std::uint32_t get_file_crc32(const std::string& name);
		std::uint32_t get_file_crc32(vpk_file_handle handle);

	};

	class vpk_search
	{
	private:
		vpk_archive* m_archive;
		vpk_file_handle m_start;
		vpk_file_handle m_end;
	public:
		vpk_search(vpk_file_handle start, vpk_file_handle end, vpk_archive* archive) :
            m_start(start),
			m_end(end),
			m_archive(archive)
		{
		}

		struct Iterator
		{
		private:
			vpk_file_handle m_handle;
			vpk_search& m_search;
		public:
			using iterator_category = std::forward_iterator_tag;
			using difference_type = std::ptrdiff_t;
			using value_type = vpk_file_handle;
			using pointer = vpk_file_handle*;
			using reference = vpk_file_handle&;

			Iterator(value_type handle, vpk_search& search) : m_handle(handle), m_search(search) {};

			std::pair<value_type, const std::string> operator*() const { return {m_handle, m_search.m_archive->get_file_name(m_handle)}; };
			pointer operator->() { return &m_handle; };
			Iterator& operator++() { m_handle++; return *this; };
			Iterator operator++(int) { Iterator t = *this; ++(*this); return t; };
			friend bool operator==(const Iterator& a, const Iterator& b) { return a.m_handle == b.m_handle; };
			friend bool operator!=(const Iterator& a, const Iterator& b) { return a.m_handle != b.m_handle; };
		};

		Iterator begin() { return Iterator(m_start, *this); };
		Iterator end() { return Iterator(m_end, *this); };
	};
}

#ifdef VPKLIB_IMPLEMENTATION

namespace vpklib {

constexpr size_t MAX_TOKEN_STRING = 512;

vpk_archive::~vpk_archive() {
	for(int i = 0; i < m_maxPakIndex; i++) {
		if(m_fileHandles[i]) {
			fclose(m_fileHandles[i]);
		}
	}
}

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
		
		if(header.version >= 2) {
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

					std::string fullName;
					// Directory will be a single space if it's the root
					if (directory[0] != ' ' && directory[1] != '\0') {
						fullName = directory;
						fullName.append("/");
					}
					fullName.append(filename);
					fullName.append(".");
					fullName.append(extension);
					m_handles.insert({fullName, key});

					// Add to our "dict"
					m_fileNames.push_back(fullName);
				}
			}
		}
		
		// skip the file data stored in this VPK so we can read actually useful stuff
		if(dirFileDataSize)
			stream.set_pos(dirFileDataSize);
		
		// Step 2: Post-dir tree and file data structures
		
		// Read the archive md5 sections 
		if(version >= 2) {
			for(size_t i = 0; i < (sectionMD5Size / sizeof(vpk2::ArchiveMD5SectionEntry)); i++) {
				m_archiveSectionEntries.push_back(stream.read<vpk2::ArchiveMD5SectionEntry>());
			}
		}
		
		// Read single OtherMD5Section
		if(version >= 2) {
			m_otherMD5Section = stream.read<vpk2::OtherMD5Section>();
		}
		
		// Read signature section 
		if(signatureSectionSize > 0 && version >= 2) {
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
	
	m_fileHandles = std::make_unique<FILE*[]>(m_maxPakIndex);

	return true;

}

vpk_archive* vpk_archive::read_from_disk(const std::filesystem::path& path) {
	auto archive = new vpk_archive();

	auto basename = path.string();
	basename.erase(basename.find("_dir.vpk"), basename.size());
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

}

#endif
