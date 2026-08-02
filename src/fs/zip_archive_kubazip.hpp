#pragma once

#include <string>

#include "utils/defs.hpp"

#include "zip_archive.hpp"

struct zip_t;

namespace fs
{

class KubaZipArchive : public ZipArchive
{
public:
    DELETE_COPY_MOVE(KubaZipArchive);
    
    void IterateFiles(IterateFilesCallback cb) override;
    std::string ReadFileAsString(const std::string& path) override;

protected:
    KubaZipArchive() = default;

protected:
    zip_t* zip_ = nullptr;

};

class FileKubaZipArchive : public KubaZipArchive
{
public:
    FileKubaZipArchive(const std::string& path);
    ~FileKubaZipArchive() override;
};

class MemoryKubaZipArchive : public KubaZipArchive
{
public:
    MemoryKubaZipArchive(std::string content);
    ~MemoryKubaZipArchive() override;

private:
    std::string content_;
};



}