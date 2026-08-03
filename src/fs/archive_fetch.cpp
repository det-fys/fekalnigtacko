#include "archive_fetch.hpp"

#include <iostream>

#include "fs/fs.hpp"
#include "version.hpp"

fs::ArchiveFetch::ArchiveFetch(const std::string& name) : name_(name)
{
#ifndef __EMSCRIPTEN__
    state_ = ArchiveFetchState::Completed;
#else

    emscripten_fetch_attr_init(&fetch_attr_);
    strcpy(fetch_attr_.requestMethod, "GET");
    fetch_attr_.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;

    fetch_attr_.onsuccess = [](emscripten_fetch_t* fetch) {
        ArchiveFetch* archive_fetch = reinterpret_cast<ArchiveFetch*>(fetch->userData);
        archive_fetch->state_ = ArchiveFetchState::Completed;
        archive_fetch->total_size_ = fetch->numBytes;
        archive_fetch->downloaded_size_ = fetch->numBytes;
        std::string content(fetch->data, fetch->numBytes);
        emscripten_fetch_close(fetch);
        archive_fetch->fetch_ = nullptr;

        try
        {
            fs::FileSystem::GetInstance().AddArchiveFromMemory(content);
        }
        catch (const std::exception& e)
        {
            archive_fetch->state_ = ArchiveFetchState::Failed;
            archive_fetch->error_ = e.what();
        }
    };

    fetch_attr_.onerror = [](emscripten_fetch_t* fetch) {
        ArchiveFetch* archive_fetch = reinterpret_cast<ArchiveFetch*>(fetch->userData);
        archive_fetch->state_ = ArchiveFetchState::Failed;
        archive_fetch->error_ = fetch->statusText;
        emscripten_fetch_close(fetch);
        archive_fetch->fetch_ = nullptr;
    };

    fetch_attr_.onprogress = [](emscripten_fetch_t* fetch) {
        ArchiveFetch* archive_fetch = reinterpret_cast<ArchiveFetch*>(fetch->userData);
        archive_fetch->downloaded_size_ = static_cast<size_t>(fetch->dataOffset);
        archive_fetch->total_size_ = static_cast<size_t>(fetch->totalBytes);
    };

    fetch_attr_.userData = this;

    std::string url = name_;
    url += "?v=";
    url += FEKAL_VERSION;

    // append time to avoid caching issues
    url += "&time=";
    url += std::to_string(time(nullptr));

    fetch_ = emscripten_fetch(&fetch_attr_, url.c_str());
    std::cout << "Started fetching archive: " << name_ << std::endl;

    state_ = ArchiveFetchState::InProgress;

#endif // __EMSCRIPTEN__
}

fs::ArchiveFetch::~ArchiveFetch()
{
#ifdef __EMSCRIPTEN__
    if (fetch_)
    {
        emscripten_fetch_close(fetch_);
        fetch_ = nullptr;
    }
#endif // __EMSCRIPTEN__
}
