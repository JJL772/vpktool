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

namespace vpklib
{
	constexpr std::uint32_t VPK_SIGNATURE = 0x55AA1234;
	constexpr std::uint16_t DIRECTORY_TERMINATOR = 0xFFFF;

	using MD5_t = char[16];
	using byte = char;

	using VPKFileHandle = std::uint64_t;

	constexpr VPKFileHandle INVALID_HANDLE = ~0ull;

	// Aliases so I don't go crazy
	template<class T>
	using UniquePtr = std::unique_ptr<T>;
	using SizeT = std::uint64_t ;

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
	namespace vpk2
	{
		constexpr std::uint32_t VERSION = 2;

		struct Header
		{
			std::uint32_t signature;
			std::uint32_t version;

			std::uint32_t tree_size;
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
			MD5_t checksum;
		} _PACKED_ATTR;

		struct OtherMD5Section
		{
			MD5_t tree_checksum;
			MD5_t archive_md5_section_checksum;
			char unknown[16];
		} _PACKED_ATTR;

#pragma pack()

		// This should not be read off the disk
		// Instead create ptrs to the pubkey and signature buffers
		struct SignatureSection
		{
			std::uint32_t pubkey_size;
			std::unique_ptr<byte> pubkey;

			std::uint32_t signature_size;
			std::unique_ptr<byte> signature;
		};
	}

	std::uint32_t get_vpk_version(const std::filesystem::path &path);
	std::uint32_t get_vpk_version(const void* mem);

	class VPK1Archive
	{
	private:
		struct File
		{
			std::int32_t archive_index = 0;

			std::uint32_t crc = 0;

			std::uint16_t preload_size = 0;
			std::unique_ptr<byte> preload_data;

			std::uint32_t offset = 0;
			std::uint32_t length = 0;

			bool dirty = false;
		};

	};


	class VPK2Archive
	{
	private:
		friend class VPK2Search;

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

		std::unordered_map<std::string, VPKFileHandle> m_handles;
		std::vector<std::string> m_fileNames; // TODO: Make this less garbage
		std::vector<std::unique_ptr<File>> m_files;

		std::unique_ptr<FILE*[]> m_fileHandles; // List of all open file handles to the individual archives
		std::uint16_t m_maxPakIndex = 0;

		std::vector<vpk2::ArchiveMD5SectionEntry> m_archiveSectionEntries;
		vpk2::OtherMD5Section m_otherMD5Section;
		vpk2::SignatureSection m_signatureSection;

	private:
		bool read(const void* mem, size_t size);

	public:

		static VPK2Archive* read_from_disk(const std::filesystem::path& path);

		VPKFileHandle find_file(const std::string& name);

		const std::string& base_archive_name() const { return m_baseArchiveName; };

		size_t get_file_size(const std::string& name);
		size_t get_file_size(VPKFileHandle handle);

		size_t get_file_preload_size(const std::string& name);
		size_t get_file_preload_size(VPKFileHandle handle);

		UniquePtr<char[]> get_file_preload_data(const std::string& name);
		UniquePtr<char[]> get_file_preload_data(VPKFileHandle handle);
		std::size_t get_file_preload_data(VPKFileHandle handle, void* buffer, std::size_t bufferSize);
		std::size_t get_file_preload_data(const std::string& name, void* buffer, std::size_t bufferSize);

		std::pair<SizeT, UniquePtr<char[]>> get_file_data(const std::string& name);
		std::pair<SizeT, UniquePtr<char[]>> get_file_data(VPKFileHandle handle);

		size_t get_file_count() const { return m_fileNames.size(); };

		class VPK2Search get_all_files();

		std::string get_file_name(VPKFileHandle handle);

		VPK2Search find_in_directory(const std::string& path);

	};

	class VPK2Search
	{
	private:
		VPK2Archive* m_archive;
		VPKFileHandle m_start;
		VPKFileHandle m_end;
	public:

		VPK2Search(VPKFileHandle start, VPKFileHandle end, VPK2Archive* archive) :
            m_start(start),
			m_end(end),
			m_archive(archive)
		{
		}

		struct Iterator
		{
		private:
			VPKFileHandle m_handle;
			VPK2Search& m_search;
		public:
			using iterator_category = std::forward_iterator_tag;
			using difference_type = std::ptrdiff_t;
			using value_type = VPKFileHandle;
			using pointer = VPKFileHandle*;
			using reference = VPKFileHandle&;

			Iterator(value_type handle, VPK2Search& search) : m_handle(handle), m_search(search) {};

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
