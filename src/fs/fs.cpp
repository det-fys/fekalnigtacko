#include "fs.hpp"

#include <fstream>
#include <filesystem>
#include <iostream>

#include "zip_archive_kubazip.hpp"

fs::FileSystem& fs::FileSystem::GetInstance()
{
    static FileSystem instance;
    return instance;
}

void fs::FileSystem::Init()
{
    ScanFiles();
}

bool fs::FileSystem::FileExists(const std::string& path)
{
    std::lock_guard<std::mutex> lock(mtx_);
    return GetFileInfo(path) != nullptr;
}

std::string fs::FileSystem::ReadFileAsString(const std::string& path)
{
    std::lock_guard<std::mutex> lock(mtx_);

    const FileInfo* info = GetFileInfo(path);
    if (!info)
    {
        throw std::runtime_error("File not found: " + path);
    }

    // in archive
    if (info->archive)
    {
        return info->archive->ReadFileAsString(info->path);
    }

    // physical
    return ReadPhysicalFile(info->path);
}

std::istringstream fs::FileSystem::ReadFileAsStream(const std::string& path)
{
    return std::istringstream(ReadFileAsString(path));
}

void fs::FileSystem::WriteFile(const std::string& virtual_path, std::string_view content)
{
    std::lock_guard<std::mutex> lock(mtx_);

    std::filesystem::path path(base_path_ / virtual_path);
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file.write(content.data(), content.size());

    files_[path.lexically_relative(base_path_).generic_string()] = FileInfo{nullptr, path.string()};
}

void fs::FileSystem::ScanFiles()
{
    std::lock_guard<std::mutex> lock(mtx_);

    namespace fs = std::filesystem;
    std::cout << "FS: Scanning files in: " << base_path_ << std::endl;

    for (const auto& entry : fs::recursive_directory_iterator(base_path_, fs::directory_options::skip_permission_denied))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }

        // Get path relative to "assets" and force forward slashes
        std::string physical_path = entry.path().string();

        if (entry.path().extension() == ".zip" || entry.path().extension() == ".tsrp")
        {
            std::cout << "archive: " << physical_path << std::endl;
            AddArchiveFile(physical_path);
            continue;
        }

        std::string virtual_path = entry.path().lexically_relative(base_path_).generic_string();
        std::cout << virtual_path << " -> " << physical_path << std::endl;
        files_[virtual_path] = FileInfo{nullptr, physical_path};
    }

    std::cout << "FS: Found " << files_.size() << " files." << std::endl;
}

void fs::FileSystem::AddArchiveFile(const std::string& path)
{
    auto archive = std::make_shared<FileKubaZipArchive>(path);
    ScanArchive(std::move(archive));
}

void fs::FileSystem::AddArchiveMemory(const std::string& content)
{
    auto archive = std::make_shared<MemoryKubaZipArchive>(content);
    ScanArchive(std::move(archive));
}

void fs::FileSystem::ScanArchive(std::shared_ptr<ZipArchive> archive)
{
    archive->IterateFiles([this, archive](const std::string& path) { 
        auto it = files_.find(path);
        if (it != files_.end() && it->second.archive == nullptr)
        {
            return; // physical file takes precedence
        }

        files_[path] = FileInfo{archive, path};
    });
}

const fs::FileInfo* fs::FileSystem::GetFileInfo(const std::string& path) const
{
    auto it = files_.find(path);
    if (it != files_.end())
        return &it->second;

    return nullptr;
}

std::string fs::FileSystem::ReadPhysicalFile(const std::string& path) const
{
    std::ifstream file(path, std::ios::binary);

    if (!file)
        throw std::runtime_error("File not found: " + path);

    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    std::string buffer(size, ' ');
    file.seekg(0);
    file.read(&buffer[0], size);

    return buffer;
}
