#pragma once

#include <string>
#include <cstddef>

#ifdef __EMSCRIPTEN__
#include <emscripten/fetch.h>
#endif // __EMSCRIPTEN__

#include "utils/defs.hpp"

namespace fs
{

enum class ArchiveFetchState
{
    NotStarted,
    InProgress,
    Completed,
    Failed
};

class ArchiveFetch
{
public:
    ArchiveFetch(const std::string& name);
    DELETE_COPY_MOVE(ArchiveFetch);

    const std::string& GetName() const { return name_; }
    ArchiveFetchState GetState() const { return state_; }

    size_t GetTotalSize() const { return total_size_; }
    size_t GetDownloadedSize() const { return downloaded_size_; }
    const std::string& GetError() const { return error_; }

    ~ArchiveFetch();

private:
    std::string name_;
    ArchiveFetchState state_ = ArchiveFetchState::NotStarted;

    size_t total_size_ = 0;
    size_t downloaded_size_ = 0;

    //std::string content_;
    std::string error_;

#ifdef __EMSCRIPTEN__
    emscripten_fetch_attr_t fetch_attr_;
    emscripten_fetch_t* fetch_ = nullptr;
#endif // __EMSCRIPTEN__

};


}