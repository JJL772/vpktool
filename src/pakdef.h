#pragma once

#include <string> 
#include <stddef.h> 

#pragma pack(1)

namespace
{
	static const char PAKSignature[4] = {'P','A','C','K'};
	
	struct pak_internal_file_t
	{
		bool onDisk;
		std::string src;
		void* ptr;
		size_t sz;
	};
	
	struct pak_header_t
	{
		char magic[4];
		uint32_t diroffset;
		uint32_t dirsize;
	};
	
	struct pak_entry_t
	{
		char filename[56];
		uint32_t offset;
		uint32_t size;
	};
}

#pragma pack()
