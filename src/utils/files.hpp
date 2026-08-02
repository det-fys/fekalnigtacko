#pragma once
#include <sstream>
#include <string>

#include "fs/fs.hpp"

/*
    compatibility functions
*/

namespace fs
{

inline bool FileExists(const std::string& path)
{
    return fs::FileSystem::GetInstance().FileExists(path);
}

inline std::string ReadFileAsString(const std::string& path)
{
    return fs::FileSystem::GetInstance().ReadFileAsString(path);
}

inline std::istringstream ReadFileAsStream(const std::string& path)
{
    return fs::FileSystem::GetInstance().ReadFileAsStream(path);
}

inline void WriteFile(const std::string& virtual_path, std::string_view content)
{
    fs::FileSystem::GetInstance().WriteFile(virtual_path, content);
}

} // namespace fs
