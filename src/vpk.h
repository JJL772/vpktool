/**
 *
 * vpk.h
 *
 * General VPK definitions
 *
 */
#pragma once

#include <cstdio>
#include <filesystem>
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
int get_vpk_version(std::filesystem::path path);

/**
 * @brief Returns the version of the VPK. Seeks to the beginning of the stream
 * once it's done
 * @param stream
 * @return vpk version or -1 if invalid
 */
int get_vpk_version(FILE* stream);

/**
 * @brief Returns the version of the VPK. Seeks to the beginning of the stream
 * after it's done
 * @param stream
 * @return version of vpk or -1 if not found
 */
int get_vpk_version(std::ifstream& stream);

/**
 * @brief Determines the type of file
 * @param vpk1 true if a VPK1
 * @param vpk2 true if a VPK2
 * @param wad true if a PWAD or IWAD
 */ 
void determine_file_type(std::filesystem::path, bool& vpk1, bool& vpk2, bool& wad);
void determine_file_type(std::ifstream& stream, bool& vpk1, bool& vpk2, bool& wad);
void determine_file_type(FILE* stream, bool& vpk1, bool& vpk2, bool& wad);

/* Public domain CRC32 code from http://home.thep.lu.se/~bjorn/crc/ */
void	 crc32(const void* data, size_t n_bytes, uint32_t* crc);
uint32_t crc32(const void* data, size_t sz);
} // namespace libvpk
