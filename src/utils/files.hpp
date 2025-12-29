#pragma once
#include <string>
#include <sstream>

namespace fs
{
    std::string ReadFileAsString(const std::string& path);
    std::istringstream ReadFileAsStream(const std::string& path);
}