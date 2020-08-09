
#include "vpk.h"

#include <fstream>

int libvpk::GetVPKVersion(std::filesystem::path path)
{
	std::ifstream fs(path);
	int	      ver = GetVPKVersion(fs);
	fs.close();
	return ver;
}

int libvpk::GetVPKVersion(FILE* stream)
{
	fseek(stream, 0, SEEK_SET);
	basic_vpk_hdr_t hdr;
	fread((void*)&hdr, sizeof(basic_vpk_hdr_t), 1, stream);
	fseek(stream, 0, SEEK_SET);
	return hdr.version;
}

int libvpk::GetVPKVersion(std::ifstream& stream)
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