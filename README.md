# vpktool

vpktool is a simple command-line utility used to modify VPK archives. This project is unrelated to any other projects that might be named "vpktool".

### Format Support

Below are the statuses of file formats supported by vpktool:

* vpk version 1: WIP
* vpk version 2: WIP
* doom wad files: Planned

### Installing

Releases for the tool can be found under the github releases page. 

Binary releases are provided from Windows and Linux, and no 32-bit releases will be provided (you can contact me about it if you REALLY need it).

### Building

Building is pretty simple, no special compile flags needed.

For Linux, simply run `make` in the root directory. Set `CXX` in your environment to change the C++ compiler.
It can be installed using `sudo make PREFIX=/usr/local/`

For Windows, you'll need to cross compile from WSL or a Linux distro, though you can also use Mingw/Cygwin/MSYS2 to build natively on Windows.
The procedure is the same as it is for building on Linux. Run `make -f Makefile.win` to build. 

Since there are no special flags needed for the compiler, you can import this code into any C++ project that uses C++11 or higher with no modifications.

### Usage

Run `vpktool -h` for a list of options and sample usage of the tool.

### Contributing

Patches and PRs are always welcome. Just make sure your code is clean and I'll probably accept it. Even if it's not clean, I might end up cleaning it up myself rather than denying the PR.
