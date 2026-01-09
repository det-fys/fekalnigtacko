#pragma once

#include "utils/defs.hpp"
#include <vector>
#include <vorbis/vorbisfile.h>

// class OggVorbis_File;
// class vorbis_info;

namespace audio
{

class OggFile
{
public:
    OggFile(const char* filename);
    DELETE_COPY_MOVE(OggFile)

    size_t GetNumChannels() const;
    size_t GetSampleRate() const;
    size_t GetNumSamples();

    size_t Read(char* buffer, size_t length);
    std::vector<char> ReadAll();

    ~OggFile();

private:
    void LoadInfo();

private:
    OggVorbis_File ogg_file_;
    vorbis_info* vorbis_info_ = nullptr;
};

} // namespace audio