#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unordered_map>

#include "vpk1.h"

void show_help(int exit_code = 1);

#define OP_INFO (1<<0)
#define OP_EXTRACT (1<<1)
#define OP_FIND (1<<2)
#define OP_ADD (1<<3)
#define OP_DELETE (1<<4)
#define OP_EXTRACT_SPECIFIC (1<<5)
#define OP_LIST (1<<6)
#define OP_LIST_INDEPTH (1<<7)

#define FL_VERBOSE (1<<0)
#define FL_DEBUG (1<<1)

#define CHECK_PARM_INDEX(_i) if(i + _i >= argc) { printf("%s expected a value, but got none.\n\n", arg); show_help(1); }
#define VERBOSE_LOG(...) if(flags & FL_VERBOSE) { printf( __VA_ARGS__); }

int main(int argc, char** argv)
{
	/* Flags and operations */
	unsigned int op = 0;
	unsigned int flags = 0;

	std::string file = "";
	std::string extract_dest = "";
	std::string find_term = "";
	std::unordered_map<std::string, std::string> add_items;
	std::vector<std::string> delete_items;
	std::unordered_map<std::string, std::string> extract_items;

	for(int i = 0; i < argc; i++)
	{
		char* arg = argv[i];

		/* Handle arguments */
		if(arg[0] == '-')
		{
			/* -i will show info */
			if(arg[1] == 'i')
			{
				op |= OP_INFO;
			}
			/* -h will show help */
			else if(arg[1] == 'h')
			{
				show_help(0);
			}
			/* -f searches for a file */
			else if(arg[1] == 'f')
			{
				op |= OP_FIND;
				CHECK_PARM_INDEX(1);
				find_term = argv[i+1];
			}
			/* -e is extract */
			else if(arg[1] == 'e')
			{
				op |= OP_EXTRACT;
				CHECK_PARM_INDEX(1);
				extract_dest = argv[i+1];
			}
			/* -a is add to vpk */
			else if(arg[1] == 'a')
			{
				op |= OP_ADD;
				CHECK_PARM_INDEX(2);
				/* First item: file in vpk. Second: file on disk */
				add_items.insert(std::make_pair(argv[i+1], argv[i+2]));
			}
			/* -d is used to remove from the vpk */
			else if(arg[1] == 'd')
			{
				op |= OP_DELETE;
				CHECK_PARM_INDEX(1);
				delete_items.push_back(argv[i+1]);
			}
			/* -v for verbose mode */
			else if(arg[1] == 'v')
			{
				flags |= FL_VERBOSE;
			}
			/* -s for extract specific targets */
			else if(arg[1] == 's')
			{
				op |= OP_EXTRACT_SPECIFIC;
				CHECK_PARM_INDEX(2);
				extract_items.insert({argv[i+1], argv[i+2]});
			}
			/* -l for listing files */
			else if(arg[1] == 'l')
			{
				op |= OP_LIST;
				if(arg[2] == 'l') op |= OP_LIST_INDEPTH;
			}
			else
			{
				printf("Unrecognized parameter %s\n\n", arg);
				show_help(1);
			}
		}
		else if(i == argc-1)
		{
			file = arg;
		}
	}

	/* Spew errors if no file is specified */
	if(file == "") show_help(1);

	/* Determine the file type */
	VERBOSE_LOG("Checking file type.\n");
	unsigned vpk_version = 0;
	if(IsVPK1(file)) vpk_version = 1;
	VERBOSE_LOG("File type is %u\n", vpk_version);

	switch(vpk_version)
	{
	case 1:
		CVPK1Archive* archive = CVPK1Archive::ReadArchive(file);
		
		/* Check for read errors */
		if(archive->last_error != CVPK1Archive::EError::NONE)
		{
			printf("Error while reading archive: %s\n", archive->GetLastErrorString().c_str());
			delete archive;
			exit(1);
		}

		/* Display info for VPK, if requested */
		if(op & OP_INFO)
		{
			archive->DumpInfo(stdout);
			archive->Write();
		}

		/* Handle deletions */
		if(op & OP_DELETE)
		{
			for(auto item : delete_items)
			{
				archive->RemoveFile(item);
			}
		}

		/* Handle additions */
		if(op & OP_ADD)
		{
			for(auto item : add_items)
			{
				archive->AddFile(item.first, item.second);
			}
		}

		/* List files */
		if(op & OP_LIST && !(op & OP_LIST_INDEPTH))
		{
			for(auto x : archive->GetFiles())
			{
				printf("%s/%s.%s\n", x.directory.c_str(), x.name.c_str(), x.extension.c_str());
			}
		}

		/* List files in depth */
		if(op & OP_LIST_INDEPTH)
		{
			for(auto x : archive->GetFiles())
			{
				printf("%s/%s.%s\n", x.directory.c_str(), x.name.c_str(), x.extension.c_str());
				printf("\tCRC32: %u 0x%X\n\tArchive: %u\n\tEntry offset: %u\n\tEntry Length: %u\n\tPreload bytes: %u\n", 
						x.dirent.crc, x.dirent.crc, x.dirent.archive_index, x.dirent.entry_offset, x.dirent.entry_length,
						x.dirent.preload_bytes);
			}
		}

		/* Extract */
		if(op & OP_EXTRACT_SPECIFIC)
		{
			for(auto x : extract_items)
			{
				bool ret = archive->ExtractFile(x.first, x.second);
				if(ret)
					printf("%s -> %s\n", x.first.c_str(), x.second.c_str());
				else
					printf("Failed to extract %s\n", x.first.c_str());
			}
		}
		
	}

}

void show_help(int code)
{
	printf("USAGE: vpktool -[iadef] archive.vpk\n\n");
	printf("\t-i              - Show info about the specified VPK\n");
	printf("\t-e <dir>        - Extract the vpk to the specified dir\n");
	printf("\t-f <file>       - Search for the specified pattern in the vpk\n");
	printf("\t-a <file>       - Add the file to the vpk\n");
	printf("\t-d <file>       - Remove the file from the vpk\n");
	printf("\t-s <file> <dst> - Extract the specified file to dst\n");
	printf("\t-v              - Enables verbose mode\n");
	exit(0);
}
