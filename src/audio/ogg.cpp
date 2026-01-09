#include "ogg.hpp"

#include <stdexcept>

audio::OggFile::OggFile(const char* filename)
{
    int result = ov_fopen(filename, &ogg_file_);

    if (result < 0)
    {
        throw std::runtime_error("(OGG) Failed to open OGG file: " + std::string(filename));
    }

    try
    {
        LoadInfo();
    }
    catch (const std::exception& e)
    {
        ov_clear(&ogg_file_);
        throw std::runtime_error(std::string("(OGG) Error loading OGG file info: ") + e.what());
    }
}

size_t audio::OggFile::GetNumChannels() const
{
    return vorbis_info_->channels;
}

size_t audio::OggFile::GetSampleRate() const
{
    return vorbis_info_->rate;
}

size_t audio::OggFile::GetNumSamples()
{
    return ov_pcm_total(&ogg_file_, -1);
}

size_t audio::OggFile::Read(char* buffer, size_t length)
{
    long res = ov_read(&ogg_file_, buffer, static_cast<int>(length), 0, 2, 1, NULL);

    if (res < 0)
    {
        throw std::runtime_error("Error reading OGG file");
    }

    return static_cast<size_t>(res);
}

std::vector<char> audio::OggFile::ReadAll()
{
    size_t total_size = ov_pcm_total(&ogg_file_, -1) * vorbis_info_->channels * 2;
    std::vector<char> buffer(total_size);

    size_t bytes_read = 0;
    while (bytes_read < total_size)
    {
        size_t bytes_to_read = total_size - bytes_read;
        size_t read = Read(buffer.data() + bytes_read, bytes_to_read);

        if (read == 0)
        {
            break; // End of file reached
        }

        bytes_read += read;
    }

    return buffer;
}

audio::OggFile::~OggFile()
{
    ov_clear(&ogg_file_); // Clear the OGG file resources
}

void audio::OggFile::LoadInfo()
{
    vorbis_info_ = ov_info(&ogg_file_, -1); // Initialize the OGG file info

    if (vorbis_info_->channels <= 0 || vorbis_info_->channels > 2)
        throw std::runtime_error("Unsupported number of channels in OGG file");
}
