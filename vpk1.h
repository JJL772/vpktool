/**
 * 
 * vpk1.h
 * 
 * Support for VPK version 1 files
 * 
 */ 
#pragma once

#include <vector>
#include <string>

#include "vpk.h"

namespace libvpk
{

	static const unsigned int VPK1Signature = 0x55AA1234;
	static const unsigned int VPK1Version = 1;
	static const unsigned short VPK1Terminator = 0xffff;

#pragma pack(1)

	struct vpk1_header_t
	{
		unsigned int signature;
		unsigned int version;
		unsigned int treesize;
	};

	struct vpk1_directory_entry_t
	{
		unsigned int crc;
		unsigned short preload_bytes;
		unsigned short archive_index;
		unsigned int entry_offset;
		unsigned int entry_length;
		unsigned short terminator;

		vpk1_directory_entry_t()
		{
			terminator = VPK1Terminator;
			crc = 0;
			preload_bytes = 0;
			archive_index = 0;
			entry_offset = 0;
			entry_length = 0;
		}
	};

#pragma pack()

	/* This structure doesnt actually exist in the file itself */
	struct vpk1_file_t
	{
		std::string name;
		std::string directory;
		std::string extension;
		std::string full_file;
		std::string srcfile;
		void *preload;
		vpk1_directory_entry_t dirent;
		/* Some flags we will use */
		bool dirty: 1;
		bool written: 1; /* Indicates if the file has been written (used in Write function) */

		vpk1_file_t()
		{
			preload = nullptr;
			dirty = false;
			written = false;
		}
	};

	/* Settings that the VPK archive will use */
	struct vpk1_settings_t
	{
		/* Set to true to keep the preload data, if it's false, preload data will be ignored */
		bool keep_preload_data;

		/* Set to true to keep all file handles to the archives open */
		bool keep_handles;

		/* Set to true to disable reading */
		bool readonly;

		/* Archive size budget in bytes
		 * If adding a file to an archive will cause the archive to be larger than this, then a new archive will be created */
		size_t size_budget;

		/* If a file is smaller than this, then the data will be written as preload data
		 * Size is in bytes */
		size_t max_preload_size;
	};

	/* Default VPK1 Settings */
	static const vpk1_settings_t DefaultVPK1Settings = {true, true, true, 512 * mb, 2048};


	class CVPK1Archive
	{
	private:
		bool readonly;
		std::vector<vpk1_file_t> files;
		std::string base_archive_name;

		vpk1_settings_t settings;

		/* Keep track of the archives on disk */
		int num_archives;
		size_t *archive_sizes;

		CVPK1Archive();

	public:
		vpk1_header_t header;
		enum EError
		{
			NONE = 0,
			INVALID_SIG,
			WRONG_VERSION,
			FILE_NOT_FOUND,
		} last_error;

		virtual ~CVPK1Archive();

		static CVPK1Archive *ReadArchive(std::string path, vpk1_settings_t settings = DefaultVPK1Settings);

		/* Returns a full list of the files in the archvie */
		const std::vector<vpk1_file_t> &GetFiles() const;

		/**
		 * @brief Removes the specified file from the archive
		 * @return true if file removed
		 */
		bool RemoveFile(std::string file);

		/**
		 * @brief Checks if the specified file exists in the archive
		 * @return true if found
		 */
		bool HasFile(std::string file) const;

		/**
		 * @brief Writes all changes to the VPK to the disk
		 * @return true if successful
		 */
		bool Write(std::string filename = "");

		/**
		 * @brief Adds a file to the archive, creating an extra VPK archive if required (not done until writing though)
		 * @param name Name of the file, in a format like this: directory/directory2/file.mdl
		 * @param data Blob of file data to be written to the VPK
		 * @param len Length of the data blob
		 * @return true if successful
		 */
		bool AddFile(std::string name, void *data, size_t len);

		/**
		 * @brief Same as above, but this will read the specified file from the disk on the fly. Use this if you wish to avoid extra memory usage
		 * @param name Name of the file to be added
		 * @param file Path to the file on disk
		 * @return true if successful
		 */
		bool AddFile(std::string name, std::string path);

		/**
		 * @brief Reads the specified file's data into a memory buffer
		 * @param file Name of the file
		 * @param buf Buffer to read into
		 * @param len Length of the buffer
		 * @return pointer to the buffer if successful
		 */
		void *ReadFile(std::string file, void *buf, size_t len);

		/**
		 * @brief Extracts the specified file to the disk
		 * @param file Name of the file to extract
		 * @param tgt Target file to extract to
		 * @return true if successful
		 */
		bool ExtractFile(std::string file, std::string tgt);

		/**
		 * @brief Returns true if the archive has been loaded
		 */
		bool Good() const
		{
			return this->last_error == EError::NONE;
		};

		/**
		 * @brief Returns a string representing the last error
		 */
		std::string GetLastErrorString() const
		{
			switch (this->last_error)
			{
				case EError::FILE_NOT_FOUND:
					return "File not found";
				case EError::INVALID_SIG:
					return "VPK signature invalid";
				case EError::WRONG_VERSION:
					return "Incorrect VPK version";
				case EError::NONE:
				default:
					return "No error";
			}
		}

		/**
		 * @brief Dumps various info about the VPK to the specified stream
		 */
		void DumpInfo(FILE *stream);

	};

}
