#include "zip_archive_kubazip.hpp"

#include <stdexcept>

#include <zip.h>

void fs::KubaZipArchive::IterateFiles(IterateFilesCallback cb)
{
    auto num_entries = zip_entries_total(zip_);

    for (size_t i = 0; i < num_entries; ++i)
    {
        if (zip_entry_openbyindex(zip_, i) != 0)
        {
            continue;
        }

        if (zip_entry_isdir(zip_) != 0 || zip_entry_issymlink(zip_) != 0)
        {
            zip_entry_close(zip_);
            continue;
        }
        
        std::string name = zip_entry_name(zip_);
        zip_entry_close(zip_);
        cb(name);
    }
}

std::string fs::KubaZipArchive::ReadFileAsString(const std::string& path)
{
    if (zip_entry_open(zip_, path.c_str()) != 0)
    {
        throw std::runtime_error("FS: Failed to open zip entry: " + path);
    }

    std::string content;

    auto result = zip_entry_extract(
        zip_,
        [](void* arg, uint64_t offset, const void* data, size_t size) -> size_t {
            std::string& content = *static_cast<std::string*>(arg);

            if (content.size() < offset + size)
            {
                content.resize(offset + size);
            }

            std::memcpy(content.data() + offset, data, size);
            return size;
        },
        &content);

    zip_entry_close(zip_);

    if (result != 0)
    {
        throw std::runtime_error("FS: Failed to extract zip entry: " + path);
    }

    return content;
}

fs::FileKubaZipArchive::FileKubaZipArchive(const std::string& path)
{
    zip_ = zip_open(path.c_str(), 0, 'r');
    if (!zip_)
    {
        throw std::runtime_error("FS: Failed to open zip file: " + path);
    }

}

fs::FileKubaZipArchive::~FileKubaZipArchive()
{
    if (zip_)
    {
        zip_close(zip_);
    }
}

fs::MemoryKubaZipArchive::MemoryKubaZipArchive(std::string content) : content_(std::move(content))
{
    zip_ = zip_stream_open(content_.data(), content_.size(), 0, 'r');
    if (!zip_)
    {
        throw std::runtime_error("FS: Failed to open zip from memory");
    }

}

fs::MemoryKubaZipArchive::~MemoryKubaZipArchive()
{
    if (zip_)
    {
        zip_stream_close(zip_);
    }
}
