#pragma once

#include <string>

namespace libvpk
{
	std::string ExtractDirectory(const std::string& fullpath);
	std::string ExtractFileExtension(const std::string& fullpath);
}
