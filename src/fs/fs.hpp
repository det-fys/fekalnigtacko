#pragma once

#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <string_view>
#include <memory>
#include <mutex>

#include "zip_archive.hpp"

namespace fs
{

struct FileInfo
{
    std::shared_ptr<ZipArchive> archive; // if null, then physical file
    std::string path;                    // path inside the archive or physical file path
};

class FileSystem
{
public:
    static FileSystem& GetInstance();

    void Init();

    bool FileExists(const std::string& path);
    std::string ReadFileAsString(const std::string& path);
    std::istringstream ReadFileAsStream(const std::string& path);
    void WriteFile(const std::string& virtual_path, std::string_view content);
    
    void AddArchiveFromMemory(const std::string& content);

    static std::string ReadPhysicalFile(const std::string& path);

private:
    FileSystem() = default;

    void ScanFiles();
    void ScanFiles(std::filesystem::path base_path);
    void AddArchiveFile(const std::string& path);
    void AddArchiveMemory(const std::string& content);
    void ScanArchive(std::shared_ptr<ZipArchive> archive);

    const FileInfo* GetFileInfo(const std::string& path) const;

private:
    std::mutex mtx_;
    std::filesystem::path base_path_ = ".";
    std::map<std::string, FileInfo> files_;
};

} // namespace fs
