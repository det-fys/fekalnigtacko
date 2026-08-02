#pragma once

#include <string>
#include <functional>

namespace fs
{

using IterateFilesCallback = std::function<void(const std::string&)>;

class ZipArchive
{
public:
    virtual void IterateFiles(IterateFilesCallback cb) = 0;
    virtual std::string ReadFileAsString(const std::string& path) = 0;

    virtual ~ZipArchive() = default;
};


}