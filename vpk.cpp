
#include "vpk.h"

#include <fstream>

int libvpk::GetVPKVersion(std::filesystem::path path)
{
	std::ifstream fs(path);
	int ver = GetVPKVersion(fs);
	fs.close();
	return ver;
}

int libvpk::GetVPKVersion(FILE *stream)
{
	fseek(stream, 0, SEEK_SET);
	basic_vpk_hdr_t hdr;
	fread((void*)&hdr, sizeof(basic_vpk_hdr_t), 1, stream);
	fseek(stream, 0, SEEK_SET);
	return hdr.version;
}

int libvpk::GetVPKVersion(std::ifstream &stream)
{
	stream.seekg(0);
	basic_vpk_hdr_t hdr;
	stream.read((char*)&hdr, sizeof(basic_vpk_hdr_t));
	stream.seekg(0);
	return hdr.version;
}
