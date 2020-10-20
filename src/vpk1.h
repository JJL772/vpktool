/**
 *
 * vpk1.h
 *
 * Support for VPK version 1 files
 *
 */
#pragma once

#include <string>
#include <vector>

#include "basearchive.h"
#include "vpk.h"

namespace libvpk
{
/* Settings that the VPK archive will use */
struct vpk1_settings_t
{
	/* Set to true to keep the preload data, if it's false, preload data
	 * will be ignored */
	bool keep_preload_data;

	/* Set to true to keep all file handles to the archives open */
	bool keep_handles;

	/* Set to true to disable reading */
	bool readonly;

	/* Archive size budget in bytes
	 * If adding a file to an archive will cause the archive to be larger
	 * than this, then a new archive will be created */
	size_t size_budget;

	/* If a file is smaller than this, then the data will be written as
	 * preload data Size is in bytes */
	size_t max_preload_size;
};

/* Default VPK1 Settings */
static const vpk1_settings_t DefaultVPK1Settings = {true, true, true, 512 * mb, 2048};

class CVPK1Archive : public IBaseArchive
{
private:
	bool			    readonly;
	std::vector<archive_file_t> files;
	std::string		    base_archive_name;

	vpk1_settings_t settings;

	/* Keep track of the archives on disk */
	int	num_archives;
	size_t* archive_sizes;

public:
	CVPK1Archive();

	vpk1_header_t header;
	enum EError
	{
		NONE = 0,
		INVALID_SIG,
		WRONG_VERSION,
		FILE_NOT_FOUND,
	} last_error;

	virtual ~CVPK1Archive();

	static CVPK1Archive* read(std::string path, vpk1_settings_t settings = DefaultVPK1Settings);

	/* Returns a full list of the files in the archvie */
	virtual const std::vector<archive_file_t>& get_files() const override { return files; }

	/**
	 * @brief Removes the specified file from the archive
	 * @return true if file removed
	 */
	virtual bool remove_file(const std::string& file) override;

	/**
	 * @brief Checks if the specified file exists in the archive
	 * @return true if found
	 */
	virtual bool contains(const std::string& file) const override;

	/**
	 * @brief Writes all changes to the VPK to the disk
	 * @return true if successful
	 */
	virtual bool write(const std::string& filename = "") override;

	/**
	 * @brief Adds a file to the archive, creating an extra VPK archive if
	 * required (not done until writing though)
	 * @param name Name of the file, in a format like this:
	 * directory/directory2/file.mdl
	 * @param data Blob of file data to be written to the VPK
	 * @param len Length of the data blob
	 * @return true if successful
	 */
	virtual bool add_file(const std::string& name, void* data, size_t len) override;

	/**
	 * @brief Same as above, but this will read the specified file from the
	 * disk on the fly. Use this if you wish to avoid extra memory usage
	 * @param name Name of the file to be added
	 * @param file Path to the file on disk
	 * @return true if successful
	 */
	virtual bool add_file(const std::string& name, const std::string& path) override;

	/**
	 * @brief Reads the specified file's data into a memory buffer
	 * @param file Name of the file
	 * @param buf Buffer to read into
	 * @param len Length of the buffer
	 * @return pointer to the buffer if successful
	 */
	virtual void* read_file(const std::string& file, void* buf, size_t& len) override;

	/**
	 * @brief Extracts the specified file to the disk
	 * @param file Name of the file to extract
	 * @param tgt Target file to extract to
	 * @return true if successful
	 */
	virtual bool extract_file(const std::string& file, std::string tgt) override;

	/**
	 * @brief Dumps various info about the VPK to the specified stream
	 */
	virtual void dump_info(FILE* stream) override;

	/**
	 * @brief Returns true if the archive has been loaded
	 */
	virtual bool good() const override { return this->last_error == EError::NONE; };

	/**
	 * @brief Returns a string representing the last error
	 */
	virtual std::string get_last_error_string() const override
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

	virtual size_t file_size(const std::string& file);

};

} // namespace libvpk
