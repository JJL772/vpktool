
#include <iostream>
#include <chrono>
#include <regex>
#include <glob.h>

#include "vpk.hpp"

#include "ezOptionParser.hpp"

void list(vpklib::vpk2_archive* archive, bool details);
void info(vpklib::vpk2_archive* archive);
void extract(vpklib::vpk2_archive* archive, ez::ezOptionParser& parser);

int main(int argc, const char** argv)
{
	ez::ezOptionParser opt;
	
	auto usage = [&](int exitCode){
		std::string u;
		opt.getUsage(u);
		printf("%s\n", u.c_str());
		exit(exitCode);
	};
	
	opt.add(
		"", 0, 0, 0,
		"Display help message",
		"-h", "--help"  
	);
	
	opt.add(
		"", 0, 0, 0,
		"List files in the archive",
		"-l", "--list"
	);
	
	opt.add(
		"", 0, 0, 0, 
		"Display details when listing files",
		"-d", "--details"
	);
	
	opt.add(
		"", 0, 0, 0,
		"Display basic info about the archive", 
		"-i", "--info"
	);
	
	opt.add(
		"", 0, -1, ',',
		"Extract the entire archive or a specified file (using regexp)",
		"-x", "--extract"
	);
	
	opt.add(
		"", 0, 1, 0,
		"Destination directory to put extracted files",
		"-o", "--outdir"
	);
	
	opt.add(
		"", 0, 1, 0,
		"Find a file in the archive",
		"-f", "--find"
	);
	
	//----------------------------------------------------------//
	
	opt.parse(argc, argv);
	
	if(opt.isSet("--help")) {
		usage(0);
		return 0;
	}
	
	if(opt.lastArgs.size() < 1) {
		fprintf(stderr, "ERROR: Expected archive name\n");
		usage(1);
		return 1;
	}
	
	// Open the archive
	auto archive = vpklib::vpk2_archive::read_from_disk(opt.lastArgs[0]->c_str());
	
	if(!archive) {
		fprintf(stderr, "ERROR: Failed to open %s\n", opt.lastArgs[0]->c_str());
		return 1;
	}
	
	bool detailed = opt.isSet("--details");
	
	if(opt.isSet("--list")) {
		list(archive, detailed);
		return 0;
	}
	
	if(opt.isSet("--info")) {
		info(archive);
		return 0;
	}
	
	if(opt.isSet("-x")) {
		extract(archive, opt);
		return 0;
	}
	

	delete archive;
	return 0;
}

void list(vpklib::vpk2_archive* archive, bool details) {
	
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

void info(vpklib::vpk2_archive* archive) {
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
		auto s = archive->get_signature();
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
		auto k = archive->get_pubkey();
		for(auto i = 0; i < pubsize; i++) {
			printf("0x%X ", k[i] & 0xFF);
		}
		puts("\n");
	}
}

void extract(vpklib::vpk2_archive* archive, ez::ezOptionParser& parser) {
	auto grp = parser.get("-x");
	
	// Build list of regexp 
	std::vector<std::string> searches;
	grp->getStrings(searches);
	std::vector<std::regex> expressions;
	try {
		for(const auto& s : searches) {
			expressions.push_back(
				std::regex(s.c_str(), std::regex_constants::ECMAScript)
			);
		}
	}
	catch(std::regex_error& e) {
		printf("ERROR: regular expression\n");
		exit(1);
	}
	
	// List of files to extract... 
	std::vector<vpklib::vpk_file_handle> toExtract; 
	
	// Wow this is trash! Nested loops and regexp everywhere, ugh
	auto search = archive->get_all_files();
	for(auto [fh, name] : search) {
		for(auto& r : expressions) {
			if(std::regex_match(name, r)) {
				toExtract.push_back(fh);
				break;
			}
		}
	}
	
	// Grab the destination directory 
	ez::OptionGroup* grp = nullptr;
	if(!(grp = parser.get("-o"))) {
		printf("ERROR: --output was not specified\n");
		exit(1);
	}
	
	std::string out;
	grp->getString(out);
	
	for(auto& x : toExtract) {
		auto data = archive->get_file_data(x);
		
		
		
	}
}
