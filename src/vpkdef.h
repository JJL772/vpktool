#pragma once
#include <string>

namespace libvpk
{
static const unsigned int   VPK1Signature  = 0x55AA1234;
static const unsigned int   VPK1Version	   = 1;
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
	unsigned int   crc;
	unsigned short preload_bytes;
	unsigned short archive_index;
	unsigned int   entry_offset;
	unsigned int   entry_length;
	unsigned short terminator;

	vpk1_directory_entry_t()
	{
		terminator    = VPK1Terminator;
		crc	      = 0;
		preload_bytes = 0;
		archive_index = 0;
		entry_offset  = 0;
		entry_length  = 0;
	}
};

#pragma pack()

/* This structure doesnt actually exist in the file itself */
struct vpk1_file_t
{
	std::string		      full_file;
	std::string		      srcfile;
	void*			      preload;
	struct vpk1_directory_entry_t dirent;
	/* Some flags we will use */
	bool dirty : 1;
	bool written : 1; /* Indicates if the file has been written (used in
			     Write function) */

	vpk1_file_t() : preload(nullptr), dirty(false), written(false) {}
};

} // namespace libvpk
