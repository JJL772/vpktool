
#include <iostream>
#include <chrono>
#include <regex>
#include <glob.h>
#include <fstream>
#include <iostream>
#include <chrono>

#include "vpk.hpp"

#include "argparse.hpp"

static bool vpk_process(const std::string& archivePath, argparse::ArgumentParser& parser);
static void vpk_list(vpklib::vpk_archive* archive, bool details);
static void vpk_info(vpklib::vpk_archive* archive);
static bool vpk_extract(vpklib::vpk_archive* archive, argparse::ArgumentParser& parser);

int main(int argc, const char** argv)
{
	argparse::ArgumentParser parser("vpktool");

	parser.add_argument("-l", "--list")
		.help("List files in the archive")
		.implicit_value(true)
		.default_value(false);
	parser.add_argument("-d", "--details")
		.help("Display additional details when listing files")
		.implicit_value(true)
		.default_value(false);
	parser.add_argument("-i", "--info")
		.help("Display basic info about the archive")
		.implicit_value(true)
		.default_value(false);
	parser.add_argument("-h", "--help")
		.help("Display help text")
		.implicit_value(true)
		.default_value(false);
	parser.add_argument("-x", "--extract")
		.help("Extract the entire archive, or a specified file. Matches via regexp")
		.implicit_value(true)
		.default_value(false);
	parser.add_argument("-p", "--pattern")
		.help("Regexp patterns to match files against when extracting")
		.default_value(std::vector<std::string>{})
		.nargs(argparse::nargs_pattern::at_least_one);
	parser.add_argument("-o", "--outdir")
		.help("Output directory to place the extracted files in")
		.nargs(1);
	parser.add_argument("-f", "--find")
		.help("Find a file in the archive")
		.nargs(argparse::nargs_pattern::at_least_one);
	parser.add_argument("files")
		.remaining()
		.help("VPK archives to process")
		.required();

	parser.parse_args(argc, argv);
	
	auto usage = [&](int exitCode){
		parser.print_help();
		exit(exitCode);
	};
	
	//----------------------------------------------------------//
	
	if(parser.get<bool>("-h")) {
		usage(0);
		return 0;
	}

	if (!parser.is_used("files")) {
		usage(1);
		return 1;
	}

	auto archives = parser.get<std::vector<std::string>>("files");

	// Missing archive name
	if(archives.size() < 1) {
		fprintf(stderr, "ERROR: Expected archive name\n");
		usage(1);
		return 1;
	}

	for (auto& pak : archives) {
		if (!vpk_process(pak, parser))
			return 1;
	}
	
	return 0;
}

static bool vpk_process(const std::string& archivePath, argparse::ArgumentParser& parser) {
	// Open the archive
	auto archive = vpklib::vpk_archive::read_from_disk(archivePath.c_str());
	
	if(!archive) {
		fprintf(stderr, "ERROR: Failed to open archive '%s'\n", archivePath.c_str());
		return false;
	}
	
	bool detailed = parser.get<bool>("--details");
	
	bool ok = true;
	if(parser.get<bool>("--list")) {
		vpk_list(archive, detailed);
	}
	
	if(parser.get<bool>("--info")) {
		vpk_info(archive);
	}
	
	if(parser.is_used("-x")) {
		auto start = std::chrono::steady_clock::now();
		ok = vpk_extract(archive, parser);
		auto end = std::chrono::steady_clock::now();
		printf("Processed %s in %.2f seconds\n", archivePath.c_str(), 
			std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count() / 1000.f );
	}

	delete archive;
	return ok;
}

// List files in the VPK, optionally with extra detail
static void vpk_list(vpklib::vpk_archive* archive, bool details) {
	
	auto search = archive->get_all_files();
	for(const auto& [fh, name] : search) {
		printf("%s\n", name.c_str());
		if(details) {
			printf("  Size: %ld\n", archive->get_file_size(fh));
			printf("  Preload size: %ld\n", archive->get_file_preload_size(fh));
			printf("  Archive index: %ld\n", archive->get_file_archive_index(fh));
			printf("  CRC32: 0x%X\n", archive->get_file_crc32(fh));
		}
	}
	
}

// Display general info about the VPK
static void vpk_info(vpklib::vpk_archive* archive) {
	printf("Version: %d\n", archive->get_version());
	printf("File count: %ld\n", archive->get_file_count());
	printf("Base archive name: %s\n", archive->base_archive_name().c_str());
	
	auto sigsize = archive->get_signature_size();
	printf("Signature size: %ld\n", sigsize);
	if(sigsize == 0) {
		printf("Signature: No signature\n");
	}
	else {
		printf("Signature: ");
		auto s = static_cast<char*>(archive->get_signature());
		for(auto i = 0; i < sigsize; i++) {
			printf("0x%X ", s[i] & 0xFF);
		}
		puts("\n");
	}
	
	auto pubsize = archive->get_pubkey_size();
	printf("Pubkey size: %ld\n", pubsize);
	if(pubsize == 0) {
		printf("Pubkey: No public key\n");
	}
	else {
		printf("Pubkey: ");
		auto k = static_cast<char*>(archive->get_pubkey());
		for(auto i = 0; i < pubsize; i++) {
			printf("0x%X ", k[i] & 0xFF);
		}
		puts("\n");
	}
}

// Extract some files from a VPK
static bool vpk_extract(vpklib::vpk_archive* archive, argparse::ArgumentParser& parser) {

	// Build list of regexp 
	std::vector<std::string> searches = parser.get<std::vector<std::string>>("-p");
	std::vector<std::regex> expressions;
	try {
		for(const auto& s : searches) {
			expressions.push_back(
				std::regex(s.c_str(), std::regex_constants::ECMAScript)
			);
		}
	}
	catch(std::regex_error& e) {
		printf("ERROR: regular expression invalid: %s\n", e.what());
		return false;
	}

	// Grab the destination directory 
	std::filesystem::path outDirPath;
	if(!parser.is_used("-o")) {
		// Default to the name of the pak file, minus ext
		outDirPath = archive->base_archive_name();
		outDirPath = outDirPath.filename();
	}
	else {
		outDirPath = parser.get<std::string>("-o");
	}

	auto extractFile = [archive, &outDirPath](vpklib::vpk_file_handle x) -> bool {
		auto data = archive->get_file_data(x);

		if (!std::get<0>(data))
			return false;
		
		// Get a full name of the file
		std::filesystem::path name = archive->get_file_name(x);
		
		std::filesystem::create_directories(outDirPath / name.parent_path());

		auto outDir = outDirPath / name;

		std::ofstream stream(outDir, std::ios::binary);
		if (!stream.good())
			return false;

		stream.write(static_cast<const char*>(std::get<0>(data)), std::get<1>(data));
		stream.close();

		std::cout << name << " -> " << outDir << "\n";

		return true;
	};
	
	// Wow this is trash! Nested loops and regexp everywhere, ugh
	auto search = archive->get_all_files();
	bool hasRegex = expressions.size();
	for(auto [fh, name] : search) {

		// Regexp processing
		if (hasRegex) {
			for(auto& r : expressions) {
				if(std::regex_match(name, r)) {
					if (!extractFile(fh))
						return false;
					break;
				}
			}
		}
		// No regexp provided
		else {
			if (!extractFile(fh))
				return false;
		}

	}
	
	return true;
}
