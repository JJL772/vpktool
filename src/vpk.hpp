#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include <tuple>
#include <set>

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
		
		std::set<std::string> m_dirs;

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
		
		/**
		 * @brief Returns the list of directories in the VPK
		 * @return const std::vector<std::string>& Directory list
		 */
		const std::set<std::string>& get_directories() const;

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
