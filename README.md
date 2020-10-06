# vpktool

vpktool is a simple command-line utility used to modify VPK archives. This project is unrelated to any other projects that might be named "vpktool".

### Format Support

Below are the statuses of file formats supported by vpktool:

* VPK Version 1: WIP
* VPK Version 2: WIP
* DOOM WAD files: Complete
* Quake PAK files: WIP

### Installing

Releases for the tool can be found under the github releases page. 

Binary releases are provided from Windows and Linux, and no 32-bit releases will be provided (you can contact me about it if you REALLY need it).

### Building

Building is pretty simple, no special compile flags needed.

To build on Linux (Or Windows!):

```sh
cmake ../
make -j8 # For windows, just open the generated SLN in visual studio
```

### Usage

Run `vpktool -h` for a list of options and sample usage of the tool.

### Contributing

Patches and PRs are always welcome. Just make sure your code is clean and I'll probably accept it. Even if it's not clean, I might end up cleaning it up myself rather than denying the PR.
