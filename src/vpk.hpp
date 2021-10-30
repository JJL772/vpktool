#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>

#ifdef _MSC_VER
#define _PACKED_ATTR
#else
#define _PACKED_ATTR __attribute__((packed))
#endif

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
		} _PACKED_ATTR;

		struct DirectoryEntry
		{
			std::uint32_t crc;
			std::uint16_t preload_bytes;
			std::uint16_t archive_index;
			std::uint32_t entry_offset;
			std::uint32_t entry_length;
		} _PACKED_ATTR;
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
		} _PACKED_ATTR;
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
		} _PACKED_ATTR;

		struct DirectoryEntry
		{
			std::uint32_t crc;
			std::uint16_t preload_bytes;
			std::uint16_t archive_index;
			std::uint32_t entry_offset;
			std::uint32_t entry_length;
			std::uint16_t terminator;
		} _PACKED_ATTR;

		struct ArchiveMD5SectionEntry
		{
			std::uint32_t archive_index;
			std::uint32_t start_offset;
			std::uint32_t count;
			md5_t		  checksum;
		} _PACKED_ATTR;

		struct OtherMD5Section
		{
			md5_t tree_checksum;
			md5_t archive_md5_section_checksum;
			char unknown[16];
		} _PACKED_ATTR;

#pragma pack()

		// This should not be read off the disk
		// Instead create ptrs to the pubkey and signature buffers
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
		 * 
		 * @param name Path to the file or a handle 
		 * @return ptr Pointer to the data
		 */
		std::unique_ptr<char[]> get_file_preload_data(const std::string& name);
		std::unique_ptr<char[]> get_file_preload_data(vpk_file_handle handle);
		
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
		 * If there is preload data associated with the file, it is concatenated with the actual data
		 * @param name Path to file or handle of file
		 * @return std::pair<SizeT, std::unique_ptr<char[]>> First item in pair is the size of the data, second item is shared ptr to data
		 */
		std::pair<std::size_t, std::shared_ptr<char[]>> get_file_data(const std::string& name);
		std::pair<std::size_t, std::shared_ptr<char[]>> get_file_data(vpk_file_handle handle);
		
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
		std::unique_ptr<char[]> get_pubkey();
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
		std::unique_ptr<char[]> get_signature();
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
