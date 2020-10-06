#include <chrono>
#include <filesystem>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unordered_map>
#include <variant>

#include "vpk.h"
#include "vpk1.h"
#include "wad.h"

using namespace libvpk;

/* Makes things a wee bit nicer... */
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::time_point<Clock>	   Time;

namespace filesystem = std::filesystem;

void show_help(int exit_code = 1);

#define OP_INFO		    (unsigned)(1 << 0)
#define OP_EXTRACT	    (unsigned)(1 << 1)
#define OP_FIND		    (unsigned)(1 << 2)
#define OP_ADD		    (unsigned)(1 << 3)
#define OP_DELETE	    (unsigned)(1 << 4)
#define OP_EXTRACT_SPECIFIC (unsigned)(1 << 5)
#define OP_LIST		    (unsigned)(1 << 6)
#define OP_LIST_INDEPTH	    (unsigned)(1 << 7)
#define OP_CREATE_ARCHIVE   (unsigned)(1 << 8)

#define FL_VERBOSE (unsigned)(1 << 0)
#define FL_DEBUG   (unsigned)(1 << 1)
#define FL_TIMED   (unsigned)(1 << 2)
#define FL_RECURSE (unsigned)(1 << 3)

#define CHECK_PARM_INDEX(_i)                                                                                                                         \
	if (i + _i >= argc)                                                                                                                          \
	{                                                                                                                                            \
		printf("%s expected a value, but got none.\n\n", arg);                                                                               \
		show_help(1);                                                                                                                        \
	}
#define VERBOSE_LOG(...)                                                                                                                             \
	if (flags & FL_VERBOSE)                                                                                                                      \
	{                                                                                                                                            \
		printf(__VA_ARGS__);                                                                                                                 \
	}

int main(int argc, char** argv)
{
	/* Flags and operations */
	unsigned int op	   = 0;
	unsigned int flags = 0;

	std::string				     file	  = "";
	std::string				     extract_dest = "";
	std::string				     find_term	  = "";
	std::unordered_map<std::string, std::string> add_items;
	std::vector<std::string>		     delete_items;
	std::unordered_map<std::string, std::string> extract_items;
	bool					     modified = false;
	std::string				     type_string = "";
	std::string				     filename;
	std::vector<std::string>		     create_files;
	std::string				     manifest; /* JSON manifest of all the files in the archive */

	Time tBeforeCheckType, tAfterCheckType;
	Time tBeforeRead, tAfterRead;
	Time tBeforeWrite, tAfterWrite;

	for (int i = 0; i < argc; i++)
	{
		char* arg = argv[i];

		/* Handle arguments */
		if (arg[0] == '-')
		{
			for (int g = 1; g < strlen(arg); g++)
			{
				/* -i will show info */
				if (arg[g] == 'i')
				{
					op |= OP_INFO;
				}
				/* -h will show help */
				else if (arg[g] == 'h')
				{
					show_help(0);
				}
				/* -f searches for a file */
				else if (arg[g] == 'f')
				{
					op |= OP_FIND;
					CHECK_PARM_INDEX(1);
					find_term = argv[i + 1];
				}
				/* -e is extract */
				else if (arg[g] == 'e')
				{
					op |= OP_EXTRACT;
					CHECK_PARM_INDEX(1);
					extract_dest = argv[i + 1];
				}
				/* -a is add to vpk */
				else if (arg[g] == 'a')
				{
					op |= OP_ADD;
					CHECK_PARM_INDEX(2);
					/* First item: file in vpk. Second: file on disk
					 */
					add_items.insert(std::make_pair(argv[i + 1], argv[i + 2]));
				}
				/* -d is used to remove from the vpk */
				else if (arg[g] == 'd')
				{
					op |= OP_DELETE;
					CHECK_PARM_INDEX(1);
					delete_items.push_back(argv[i + 1]);
				}
				/* -v for verbose mode */
				else if (arg[g] == 'v')
				{
					flags |= FL_VERBOSE;
				}
				/* -s for extract specific targets */
				else if (arg[g] == 's')
				{
					op |= OP_EXTRACT_SPECIFIC;
					CHECK_PARM_INDEX(2);
					extract_items.insert({argv[i + 1], argv[i + 2]});
				}
				/* -t for timing */
				else if (arg[g] == 't')
				{
					flags |= FL_TIMED;
				}
				/* -l for listing files */
				else if (arg[g] == 'l')
				{
					if (op & OP_LIST)
						op |= OP_LIST_INDEPTH;
					op |= OP_LIST;
				}
				/* -r for recursive adding */
				else if (arg[g] == 'r')
				{
					flags |= FL_RECURSE;
				}
				/* -c for creating an archive */
				else if (arg[g] == 'c')
				{
					op |= OP_CREATE_ARCHIVE;
					CHECK_PARM_INDEX(1);
					filename = argv[i + 1];
					i++;
				}
				/* -m for the archive type */
				else if (arg[g] == 'm')
				{
					CHECK_PARM_INDEX(1);
					type_string = std::string(argv[i + 1]);
					i++;
				}
				else
				{
					printf("Unrecognized parameter %s\n\n", arg);
					show_help(1);
				}
			}
		}
		else if (i == argc - 1 && i > 0 && !(op & OP_CREATE_ARCHIVE))
		{
			file = arg;
		}
		else if (op & OP_CREATE_ARCHIVE)
		{
			/* Everything else is appended into the create_files listing if OP_CREATE_ARCHIVE is set. */
			create_files.push_back(argv[i]);
		}
	}

	/* Spew errors if no file is specified */
	if (file == "" && !(op & OP_CREATE_ARCHIVE))
		show_help(1);

	std::filesystem::path filepath(file);

	if (!std::filesystem::exists(filepath) && !(op & OP_CREATE_ARCHIVE))
	{
		printf("File %s does not exist\n", filepath.c_str());
		exit(1);
	}

	bool bVPK1 = false;
	bool bVPK2 = false;
	bool bWAD = false;
	bool bPAK = false;

	if (!(op & OP_CREATE_ARCHIVE))
	{
		tBeforeCheckType = Clock::now();
		libvpk::determine_file_type(file, bVPK1, bVPK2, bWAD);
		tAfterCheckType = Clock::now();
	}
	else
	{
		if (type_string == "vpk1")
		{
			bVPK1 = true;
			VERBOSE_LOG("File is a VPK1\n");
		}
		else if (type_string == "vpk2")
		{
			bVPK2 = true;
			VERBOSE_LOG("File is a VPK2\n");
		}
		else if (type_string == "wad")
		{
			bWAD = true;
			VERBOSE_LOG("File is a WAD\n");
		}
		else if(type_string == "pak")
		{
			bPAK = true;
			VERBOSE_LOG("File is a PAK\n");
		}
	}

	/* Check if invalid */
	if (!bVPK1 && !bVPK2 && !bWAD)
	{
		printf("File %s is not a VPK or WAD File\n", filename.c_str());
		exit(1);
	}

	/* Archive is new */
	if (op & OP_CREATE_ARCHIVE)
	{
		IBaseArchive* archive = nullptr;
		if (bWAD)
			archive = new CWADArchive();

		/* File manifests will automatically override recurse flags */
		if (manifest != "")
		{
		}
		/* If the -r flag is specified, the file list should be fully made of directories */
		else if (flags & FL_RECURSE)
		{
			std::vector<std::string> actual_files;

			/* Loop through the dirs and recursively add files to the real file list */
			for (auto file : create_files)
			{
				if (!filesystem::is_directory(file))
				{
					printf("%s is not a directory.\n", file.c_str());
					exit(1);
				}

				/* Recurse through directories and add all files */
				for (auto& p : filesystem::recursive_directory_iterator(file))
				{
					if (!p.is_directory())
					{
						/* Strip off the root directory of the file when we add it */
						std::string _file = p.path().string();
						size_t	    pos	  = _file.find(file);
						if (pos != std::string::npos)
							_file.replace(pos, file.size(), "");

						if (!archive->add_file(_file, p.path().string()))
						{
							printf("Failed to add %s to the archive!\n", p.path().string().c_str());
							exit(1);
						}
						else
							VERBOSE_LOG("Added %s to archive as %s\n", p.path().string().c_str(), _file.c_str());
					}
				}
			}
		}
		else
		{
			for (const auto& _file : create_files)
			{
				VERBOSE_LOG("Added %s to archive\n", _file.c_str());
				archive->add_file(_file, _file);
			}
		}

		archive->write(filename);
	}
	/* Archive is *not* new */
	else
	{
		tBeforeRead	      = Clock::now();
		IBaseArchive* archive = nullptr;
		if (bVPK1)
			archive = CVPK1Archive::read(file);
		else if (bWAD)
			archive = CWADArchive::read(file);
		tAfterRead = Clock::now();

		if (!archive)
		{
			printf("Internal error while reading archive\n");
			exit(1);
		}

		/* Check for read errors */
		if (archive->get_last_error_string() != "")
		{
			printf("Error while reading archive: %s\n", archive->get_last_error_string().c_str());
			delete archive;
			exit(1);
		}

		/* Display info for VPK, if requested */
		if (op & OP_INFO)
		{
			archive->dump_info(stdout);
		}

		/* Handle deletions */
		if (op & OP_DELETE)
		{
			for (auto item : delete_items)
			{
				archive->remove_file(item);
			}
			modified = true;
		}

		/* Handle additions */
		if (op & OP_ADD)
		{
			for (auto item : add_items)
			{
				archive->add_file(item.first, item.second);
			}
			modified = true;
		}

		/* List files */
		if (op & OP_LIST && !(op & OP_LIST_INDEPTH))
		{
			for (const auto& x : archive->get_files())
			{
				/* WAD files do not have directories */
				if (bVPK1 || bVPK2)
					printf("%s/%s.%s\n", x.dir.c_str(), x.name.c_str(), x.ext.c_str());
				else
					printf("%s\n", x.name.c_str());
			}
		}

		/* List files in depth */
		if (op & OP_LIST_INDEPTH)
		{
			if (bVPK1)
			{
				for (const auto& x : archive->get_files())
				{
					const vpk1_file_t& vpk1_file = std::any_cast<const vpk1_file_t&>(x.internal);
					printf("%s/%s.%s\n", x.dir.c_str(), x.name.c_str(), x.ext.c_str());
					printf("\tCRC32: %u 0x%X\n\tArchive: "
					       "%u\n\tEntry offset: %u\n\tEntry "
					       "Length: %u\n\tPreload bytes: %u\n",
					       vpk1_file.dirent.crc, vpk1_file.dirent.crc, vpk1_file.dirent.archive_index,
					       vpk1_file.dirent.entry_offset, vpk1_file.dirent.entry_length, vpk1_file.dirent.preload_bytes);
				}
			}
			else if (bWAD)
			{
				for (const auto& x : archive->get_files())
				{
					printf("%s\n", x.name.c_str());
					printf("\tOffset: %u\n\tSize: %d\n", x.offset, x.size);
				}
			}
		}

		/* Extract */
		if (op & OP_EXTRACT_SPECIFIC)
		{
			for (auto x : extract_items)
			{
				bool ret = archive->extract_file(x.first, x.second);
				if (ret)
					printf("%s -> %s\n", x.first.c_str(), x.second.c_str());
				else
					printf("Failed to extract %s\n", x.first.c_str());
			}
		}

		/* If the VPK has been modified, let's write it */
		tBeforeWrite = Clock::now();
		if (modified)
			archive->write();
		tAfterWrite = Clock::now();

		delete archive;

		printf("Time to determine type: %f ms\n", (tAfterCheckType - tBeforeCheckType).count() / 1000000.0f);
		printf("Time to read: %f ms\n", (tAfterRead - tBeforeRead).count() / 1000000.0f);
		if (modified)
			printf("Time to write changes to disk: %f ms\n", (tAfterWrite - tBeforeWrite).count() / 1000000.0f);
	}
}

void show_help(int code)
{
	printf("USAGE: vpktool -[iadefllcr] archive.vpk [files...]\n\n");
	printf("\t-l[l]           - Lists all files in the archive. If a second l is specified, it lists in great detail\n");
	printf("\t-i              - Show info about the specified VPK\n");
	printf("\t-e <dir>        - Extract the vpk to the specified dir\n");
	printf("\t-f <file>       - Search for the specified pattern in the "
	       "vpk\n");
	printf("\t-a <file>       - Add the file to the vpk\n");
	printf("\t-d <file>       - Remove the file from the vpk\n");
	printf("\t-s <file> <dst> - Extract the specified file to dst\n");
	printf("\t-v              - Enables verbose mode\n");
	printf("\t-t              - Record the time of tool execution\n");
	printf("\t-r              - Recursively add files to the archive\n");
	printf("\t-c              - Creates a new archive\n");
	printf("\t-m              - Type of archive. Valid opts are: vpk1 vpk2 wad\n");
	exit(0);
}
