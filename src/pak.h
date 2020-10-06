/**
 *
 * pak.h - Support for Quake PAK files
 *
 */
#pragma once

#include <list>
#include <stdint.h>
#include <vector>

#include "basearchive.h"

namespace libvpk
{

struct pak_settings_t
{
	/* Whether or not to keep file handles open */
	bool bKeepFileHandles;
	/* Should file data be cached when the file is read? */
	bool bCacheFileData;
	/* Should all files be read into memory when the archive is loaded? */
	bool bReadToMemory;
	/* Maximum amount of data we can cache */
	size_t nCacheSize;
};

static pak_settings_t g_DefaultPAKSettings = {
	true, /* Keep file handles open */
};

class CPAKArchive : public IBaseArchive
{
private:
	pak_header_t		    m_header;
	std::string		    m_onDiskName;
	std::vector<archive_file_t> m_files;
	bool			    m_onDisk : 1; /* True if this file has been read from the disk, and it's not new */
	bool			    m_dirty : 1;
	bool			    m_error : 1;
	pak_settings_t		    m_settings;
	FILE*			    m_fileHandle;

public:
	explicit CPAKArchive(pak_settings_t settings = g_DefaultPAKSettings);
	virtual ~CPAKArchive();

	static CPAKArchive* read(const std::string& path, pak_settings_t settings = g_DefaultPAKSettings);


	/* Returns a full list of the files in the archvie */
	virtual const std::vector<archive_file_t>& get_files() const { return m_files; }

	/**
	 * @brief Removes the specified file from the archive
	 * @return true if file removed
	 */
	virtual bool remove_file(const std::string& file);

	/**
	 * @brief Checks if the specified file exists in the archive
	 * @return true if found
	 */
	virtual bool contains(const std::string& file) const;

	/**
	 * @brief Writes all changes to the VPK to the disk
	 * @param filename If set, the filename to write this file to.
	 * @return true if successful
	 */
	virtual bool write(const std::string& filename = "");

	/**
	 * @brief Adds a file to the archive, creating an extra VPK archive if
	 * required (not done until writing though)
	 * @param name Name of the file, in a format like this:
	 * directory/directory2/file.mdl
	 * @param data Blob of file data to be written to the VPK
	 * @param len Length of the data blob
	 * @return true if successful
	 */
	virtual bool add_file(const std::string& name, void* data, size_t len);

	/**
	 * @brief Same as above, but this will read the specified file from the
	 * disk on the fly. Use this if you wish to avoid extra memory usage
	 * @param name Name of the file to be added
	 * @param file Path to the file on disk
	 * @return true if successful
	 */
	virtual bool add_file(const std::string& name, const std::string& path);

	/**
	 * @brief Reads the specified file's data into a memory buffer
	 * @param file Name of the file
	 * @param buf Buffer to read into
	 * @param len Length of the buffer
	 * @return pointer to the buffer if successful
	 */
	virtual void* read_file(const std::string& file, void* buf, size_t& len);

	/**
	 * @brief Extracts the specified file to the disk
	 * @param file Name of the file to extract
	 * @param tgt Target file to extract to
	 * @return true if successful
	 */
	virtual bool extract_file(const std::string& file, std::string tgt);

	/**
	 * @brief Returns true if the archive has been loaded
	 */
	virtual bool good() const { return true; };

	/**
	 * @brief Returns a string representing the last error
	 */
	virtual std::string get_last_error_string() const { return ""; };

	/**
	 * @brief Dumps various info about the VPK to the specified stream
	 */
	virtual void dump_info(FILE* stream);

	virtual size_t file_size(const std::string& file);
};

} // namespace libvpk
