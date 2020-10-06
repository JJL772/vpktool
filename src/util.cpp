
#include "util.h"

std::string libvpk::ExtractDirectory(const std::string& fullpath) 
{
	size_t pos = fullpath.find_last_of('/');
	if(pos == std::string::npos)
		return "";
	return fullpath.substr(0, pos-1);
}

std::string libvpk::ExtractFileExtension(const std::string& fullpath) 
{
	size_t pos = fullpath.find_last_of('.');
	if(pos == std::string::npos)
		return "";
	return fullpath.substr(pos, fullpath.size()-1);
}
