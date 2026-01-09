#include "sound.hpp"

#include <AL/al.h>
#include <AL/alc.h>

#include "assets/cmdfile.hpp"
#include "utils/files.hpp"

#include "ogg.hpp"

audio::Sound::Sound()
{
    alGenBuffers(1, &buffer_);

    if (!buffer_)
        throw std::runtime_error("Failed to generate OpenAL buffer");
}

static void LoadBufferOGG(ALuint buffer, const char* path)
{
    audio::OggFile ogg_file(path);
    std::vector<char> data = ogg_file.ReadAll();

    ALenum format = ogg_file.GetNumChannels() == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
    ALsizei sample_rate = ogg_file.GetSampleRate();

    alBufferData(buffer, format, data.data(), static_cast<ALsizei>(data.size()), sample_rate);
}

std::shared_ptr<const audio::Sound> audio::Sound::LoadFromFile(const std::string& path)
{
    auto sound = std::make_shared<Sound>();

    std::string ogg_name;

    assets::LoadCMDFile(path, [&](const std::string& cmd, std::istringstream& iss) {
        if (cmd == "ogg")
        {
            iss >> ogg_name;
        }
        else if (cmd == "category")
        {
            iss >> sound->category_name_;
        }
        else if (cmd == "volume")
        {
            iss >> sound->volume_;
        }
        else if (cmd == "pitch")
        {
            iss >> sound->pitch_;
        }
    });

    if (sound->category_name_.empty())
        sound->category_name_ = "default";

    ogg_name = "data/" + ogg_name + ".ogg";
    LoadBufferOGG(sound->GetBufferId(), ogg_name.c_str());

    return sound;
}

audio::Sound::~Sound()
{
    alDeleteBuffers(1, &buffer_);
}
