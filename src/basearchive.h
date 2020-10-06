/**
 *
 * basearchive.h - Base class for all archives
 *
 */
#pragma once

#include <any>
#include <list>
#include <string>
#include <vector>

#include "vpkdef.h"
#include "waddef.h"
#include "pakdef.h"

namespace libvpk
{
struct archive_file_t
{
	std::string name;
	std::string dir; // directory, if applicable
	std::string ext; // File extension, if applicable
	size_t	    size;
	size_t	    offset;
	bool	    on_disk : 1;
	bool	    dirty : 1;

	std::any internal;

	/* Handle initialization */
	archive_file_t() : size(0), offset(0), name(""), dir(""), ext(""), on_disk(false), dirty(false) {}
	~archive_file_t();
	archive_file_t(archive_file_t&& other);
	archive_file_t(const archive_file_t& other);
	archive_file_t& operator=(const archive_file_t& other);
	archive_file_t& operator=(archive_file_t&& other);
};

class IBaseArchive
{
public:
	virtual ~IBaseArchive(){};

	/* Returns a full list of the files in the archvie */
	virtual const std::vector<archive_file_t>& get_files() const = 0;

	/**
	 * @brief Removes the specified file from the archive
	 * @return true if file removed
	 */
	virtual bool remove_file(const std::string& file) = 0;

	/**
	 * @brief Checks if the specified file exists in the archive
	 * @return true if found
	 */
	virtual bool contains(const std::string& file) const = 0;

	/**
	 * @brief Writes all changes to the VPK to the disk
	 * @return true if successful
	 */
	virtual bool write(const std::string& filename = "") = 0;

	/**
	 * @brief Adds a file to the archive, creating an extra VPK archive if
	 * required (not done until writing though)
	 * @param name Name of the file, in a format like this:
	 * directory/directory2/file.mdl
	 * @param data Blob of file data to be written to the VPK
	 * @param len Length of the data blob
	 * @return true if successful
	 */
	virtual bool add_file(const std::string& name, void* data, size_t len) = 0;

	/**
	 * @brief Same as above, but this will read the specified file from the
	 * disk on the fly. Use this if you wish to avoid extra memory usage
	 * @param name Name of the file to be added
	 * @param file Path to the file on disk
	 * @return true if successful
	 */
	virtual bool add_file(const std::string& name, const std::string& path) = 0;

	/**
	 * @brief Reads the specified file's data into a memory buffer
	 * @param file Name of the file
	 * @param buf Buffer to read into
	 * @param len Length of the buffer
	 * @return pointer to the buffer if successful
	 */
	virtual void* read_file(const std::string& file, void* buf, size_t& len) = 0;

	/**
	 * @brief Extracts the specified file to the disk
	 * @param file Name of the file to extract
	 * @param tgt Target file to extract to
	 * @return true if successful
	 */
	virtual bool extract_file(const std::string& file, std::string tgt) = 0;

	/**
	 * @brief Returns true if the archive has been loaded
	 */
	virtual bool good() const = 0;

	/**
	 * @brief Returns a string representing the last error
	 */
	virtual std::string get_last_error_string() const = 0;

	/**
	 * @brief Dumps various info about the VPK to the specified stream
	 */
	virtual void dump_info(FILE* stream) = 0;
};

} // namespace libvpk
