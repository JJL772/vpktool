/**
 *
 * vpk.h
 *
 * General VPK definitions
 *
 */
#pragma once

#include <filesystem>
#include <cstdio>
#include <iostream>

namespace libvpk
{
	static const unsigned int VPKSignature = 0x55AA1234;

	static const unsigned int kb = 1024;
	static const unsigned int mb = kb * 1024;
	static const unsigned int gb = mb * 1024;

	/* Basic VPK header */
#pragma pack(1)
	struct basic_vpk_hdr_t
	{
		unsigned int signature;
		unsigned int version;
	};
#pragma pack()

	/**
	 * @brief Returns the version of the VPK
	 * @param path
	 * @return vpk version or -1 if invalid
	 */
	int GetVPKVersion(std::filesystem::path path);

	/**
	 * @brief Returns the version of the VPK. Seeks to the beginning of the stream once it's done
	 * @param stream
	 * @return vpk version or -1 if invalid
	 */
	int GetVPKVersion(FILE* stream);

	/**
	 * @brief Returns the version of the VPK. Seeks to the beginning of the stream after it's done
	 * @param stream
	 * @return version of vpk or -1 if not found
	 */
	int GetVPKVersion(std::ifstream& stream);

	/* Public domain CRC32 code from http://home.thep.lu.se/~bjorn/crc/ */
	void crc32(const void *data, size_t n_bytes, uint32_t* crc);
	uint32_t crc32(const void* data, size_t sz);
}