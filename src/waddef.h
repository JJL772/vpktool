#pragma once
#include <string>

namespace libvpk
{

static const char PWADSignature[4] = {'P', 'W', 'A', 'D'};
static const char IWADSignature[4] = {'I', 'W', 'A', 'D'};

struct wad_internal_file_t
{
	bool	    onDisk; /* True if the file is on the disk (we use src in that case) */
	std::string src;
	void*	    ptr;
	size_t	    sz;

	wad_internal_file_t(const wad_internal_file_t& other);
	wad_internal_file_t(wad_internal_file_t&& other) noexcept;
	wad_internal_file_t();
	~wad_internal_file_t();
};

#pragma pack(1)
struct wad_header_t
{
	char	signature[4];
	int32_t entries; // Doom reads them as signed ints, so we will too
	int32_t dir_offset;
};

struct wad_directory_t
{
	int32_t offset;
	int32_t size;
	char	name[8];
};
#pragma pack()

} // namespace libvpk
