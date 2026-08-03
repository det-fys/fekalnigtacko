#include "ogg.hpp"

#include <stdexcept>

#include "utils/files.hpp"

audio::OggFile::OggFile(const std::string& path)
{
    content_ = fs::ReadFileAsString(path);
    
    ov_callbacks callbacks{};
    callbacks.seek_func = [](void* datasource, ogg_int64_t offset, int whence) -> int {
        OggFile* ogg_file = static_cast<OggFile*>(datasource);

        if (whence == SEEK_SET)
        {
            ogg_file->content_pos_ = static_cast<size_t>(offset);
        }
        else if (whence == SEEK_CUR)
        {
            ogg_file->content_pos_ += static_cast<size_t>(offset);
        }
        else if (whence == SEEK_END)
        {
            ogg_file->content_pos_ = ogg_file->content_.size() + static_cast<size_t>(offset);
        }
        else
        {
            return -1; // Invalid whence
        }

        if (ogg_file->content_pos_ > ogg_file->content_.size())
        {
            return -1; // Seeking beyond the end of the content
        }

        return 0; // Success
    };

    callbacks.tell_func = [](void* datasource) -> long {
        OggFile* ogg_file = static_cast<OggFile*>(datasource);
        return static_cast<long>(ogg_file->content_pos_);
    };

    callbacks.read_func = [](void* ptr, size_t size, size_t nmemb, void* datasource) -> size_t {
        OggFile* ogg_file = static_cast<OggFile*>(datasource);
        size_t bytes_to_read = size * nmemb;
        if (ogg_file->content_pos_ + bytes_to_read > ogg_file->content_.size())
        {
            bytes_to_read = ogg_file->content_.size() - ogg_file->content_pos_;
        }
        std::memcpy(ptr, ogg_file->content_.data() + ogg_file->content_pos_, bytes_to_read);
        ogg_file->content_pos_ += bytes_to_read;
        return bytes_to_read / size; // Return the number of elements read
    };

    callbacks.close_func = [](void* datasource) -> int {
        // No action needed for in-memory data
        return 0;
    };

    int result = ov_open_callbacks(this, &ogg_file_, nullptr, 0, callbacks);

    if (result < 0)
    {
        throw std::runtime_error("(OGG) Failed to open OGG file: " + std::string(path));
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
