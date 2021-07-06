
#include <iostream>
#include <chrono>

#include "vpk.hpp"

#include "ezOptionParser.hpp"

int main(int argc, char** argv)
{
	if(argc < 2)
		return 1;

	const char* file = argv[1];

	auto start = std::chrono::steady_clock::now();

	auto archive = vpklib::VPK2Archive::read_from_disk(file);

	auto end = std::chrono::steady_clock::now();

	if(!archive) {
		printf("Failed to load %s\n", file);
		return 1;
	}

	printf("%zu files\n", archive->get_file_count());

	auto search = archive->get_all_files();
	for(const auto& [fh, name] : search) {
		if(archive->get_file_preload_size(fh)) {
			printf("P: %s\n", name.c_str());
		}
	}

	auto mat = archive->find_file("scripts/game_sounds_passtime.txt");

	printf("Preload size: %zu\n", archive->get_file_preload_size(mat));
	printf("Preload data: %s\n", archive->get_file_preload_data(mat).get());
	printf("Signature size: %llu\n", archive->get_signature_size());


	auto dt = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
	printf("Time to read archive: %f ms\n", dt / 1e6);

	delete archive;

	return 0;


}
