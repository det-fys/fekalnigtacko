#include "files.hpp"

#include <fstream>

std::string fs::ReadFileAsString(const std::string& path)
{
    std::ifstream t(path, std::ios::binary);
    t.seekg(0, std::ios::end);
    size_t size = t.tellg();
    std::string buffer(size, ' ');
    t.seekg(0);
    t.read(&buffer[0], size); 
    return buffer;
}

std::istringstream fs::ReadFileAsStream(const std::string& path)
{
    std::string content = ReadFileAsString(path);
    return std::istringstream(content);
}
