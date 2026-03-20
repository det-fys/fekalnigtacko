#pragma once
#include <string>
#include <sstream>

namespace fs
{
    bool FileExists(const std::string& path);
    std::string ReadFileAsString(const std::string& path);
    std::istringstream ReadFileAsStream(const std::string& path);
}