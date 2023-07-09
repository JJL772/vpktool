
#define VPKLIB_IMPLEMENTATION
#include "vpk.hpp"

#include <iostream>

int main(int argc, char** argv) {
    if (argc < 2)
        return 1;

    for (int i = 1; i < argc; ++i) {
        auto file = vpklib::vpk_archive::read_from_disk(argv[i]);
        auto search = file->get_all_files();
        for (auto f : search) {
            std::cout << f.second << std::endl;
        }
        delete file;
    }
}