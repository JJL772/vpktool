/**
 *
 * wad.h - Support for DOOM wad files
 *
 */
#pragma once

#include <list>
#include <stdint.h>
#include <vector>

#include "basearchive.h"

namespace libvpk
{

struct wad_settings_t
{
	/* Whether or not to keep file handles open */
	bool bKeepFileHandles;
};

static wad_settings_t g_DefaultWadSettings = {
	true, /* Keep file handles open */
};

class CWADArchive : public IBaseArchive
{
private:
	wad_header_t		    m_header;
	std::string		    m_onDiskName;
	std::vector<archive_file_t> m_files;
	bool			    m_onDisk : 1; /* True if this file has been read from the disk, and it's not new */
	bool			    m_IWAD : 1;
	bool			    m_PWAD : 1;
	bool			    m_dirty : 1;
	bool			    m_error : 1;
	wad_settings_t		    m_settings;
	FILE*			    m_fileHandle;

	void calc_offsets();

public:
	explicit CWADArchive(wad_settings_t settings = g_DefaultWadSettings);
	virtual ~CWADArchive();

	static CWADArchive* read(const std::string& path, wad_settings_t settings = g_DefaultWadSettings);

	bool is_pwad() const { return m_PWAD; }
	bool is_iwad() const { return m_IWAD; }
	void set_pwad(bool b)
	{
		m_PWAD = b;
		m_IWAD = !b;
	}
	void set_iwad(bool b)
	{
		m_IWAD = b;
		m_PWAD = !b;
	}

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
};

} // namespace libvpk
