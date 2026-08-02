#pragma once

#include "archive_fetch.hpp"
#include "fs/fs.hpp"

fs::ArchiveFetch::ArchiveFetch(const std::string& name) : name_(name)
{
#ifndef __EMSCRIPTEN__
    state_ = ArchiveFetchState::Completed;
#else
    // TODO: Implement fetching from a remote source in Emscripten environment
    state_ = ArchiveFetchState::Failed;
    error_ = "Fetching from remote source not implemented in Emscripten.";

#endif // __EMSCRIPTEN__
}
