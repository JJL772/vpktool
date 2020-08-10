
#include "vpk.h"
#include "basearchive.h"
#include "vpk1.h"
#include "wad.h"

#include <fstream>
#include <memory.h>

using namespace libvpk;

int libvpk::get_vpk_version(std::filesystem::path path)
{
	std::ifstream fs(path);
	int	      ver = get_vpk_version(fs);
	fs.close();
	return ver;
}

int libvpk::get_vpk_version(FILE* stream)
{
	fseek(stream, 0, SEEK_SET);
	basic_vpk_hdr_t hdr;
	fread((void*)&hdr, sizeof(basic_vpk_hdr_t), 1, stream);
	fseek(stream, 0, SEEK_SET);
	return hdr.version;
}

int libvpk::get_vpk_version(std::ifstream& stream)
{
	stream.seekg(0);
	basic_vpk_hdr_t hdr;
	stream.read((char*)&hdr, sizeof(basic_vpk_hdr_t));
	stream.seekg(0);
	return hdr.version;
}

uint32_t crc32_for_byte(uint32_t r)
{
	for (int j = 0; j < 8; ++j)
		r = (r & 1 ? 0 : (uint32_t)0xEDB88320L) ^ r >> 1;
	return r ^ (uint32_t)0xFF000000L;
}

void libvpk::crc32(const void* data, size_t n_bytes, uint32_t* crc)
{
	static uint32_t table[0x100];
	if (!*table)
		for (size_t i = 0; i < 0x100; ++i)
			table[i] = crc32_for_byte(i);
	for (size_t i = 0; i < n_bytes; ++i)
		*crc = table[(uint8_t)*crc ^ ((uint8_t*)data)[i]] ^ *crc >> 8;
}

uint32_t libvpk::crc32(const void* dat, size_t n)
{
	uint32_t out = 0;
	crc32(dat, n, &out);
	return out;
}

void libvpk::determine_file_type(std::filesystem::path path, bool& vpk1, bool& vpk2, bool& wad)
{
	FILE* fs = fopen(path.c_str(), "rb");
	determine_file_type(fs, vpk1, vpk2, wad);
}

void libvpk::determine_file_type(std::ifstream& stream, bool& vpk1, bool& vpk2, bool& wad)
{
	basic_vpk_hdr_t hdr;
	stream.read((char*)&hdr, sizeof(basic_vpk_hdr_t));
	vpk1 = vpk2 = wad = false;
	if (hdr.signature == VPKSignature)
	{
		if (hdr.version == 1)
			vpk1 = true;
		else if (hdr.version == 2)
			vpk2 = true;
	}
	else if (memcmp(&hdr.signature, PWADSignature, 4) == 0 || memcmp(&hdr.signature, IWADSignature, 4) == 0)
		wad = true;
}

void libvpk::determine_file_type(FILE* stream, bool& vpk1, bool& vpk2, bool& wad)
{
	basic_vpk_hdr_t hdr;
	fread(&hdr, sizeof(basic_vpk_hdr_t), 1, stream);
	vpk1 = vpk2 = wad = false;
	if (hdr.signature == VPKSignature)
	{
		if (hdr.version == 1)
			vpk1 = true;
		else if (hdr.version == 2)
			vpk2 = true;
	}
	else if (memcmp(&hdr.signature, PWADSignature, 4) == 0 || memcmp(&hdr.signature, IWADSignature, 4) == 0)
		wad = true;
}

archive_file_t::archive_file_t(const archive_file_t& other)
{
	size	= other.size;
	offset	= other.offset;
	on_disk = other.on_disk;
	name	= other.name;
	dir	= other.dir;
	ext	= other.ext;
}

archive_file_t::archive_file_t(archive_file_t&& other)
{
	size	      = other.size;
	offset	      = other.offset;
	on_disk	      = other.on_disk;
	name	      = other.name;
	dir	      = other.dir;
	ext	      = other.ext;
	other.offset  = 0;
	other.size    = 0;
	other.name    = "";
	other.on_disk = other.dirty = false;
	other.dir		    = "";
	other.ext		    = "";
}

archive_file_t& archive_file_t::operator=(archive_file_t&& other)
{
	size	      = other.size;
	offset	      = other.offset;
	on_disk	      = other.on_disk;
	name	      = other.name;
	dir	      = other.dir;
	ext	      = other.ext;
	other.offset  = 0;
	other.size    = 0;
	other.name    = "";
	other.on_disk = other.dirty = false;
	other.dir		    = "";
	other.ext		    = "";
	return *this;
}

archive_file_t& archive_file_t::operator=(const archive_file_t& other)
{
	size	= other.size;
	offset	= other.offset;
	on_disk = other.on_disk;
	name	= other.name;
	dir	= other.dir;
	ext	= other.ext;
	return *this;
}

archive_file_t::~archive_file_t() {}
