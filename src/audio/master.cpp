#include "master.hpp"

#include <iostream>

#include "defs.hpp"
#include "sound_source.hpp"
#include "source.hpp"

static const size_t MAX_CATEGORIES = 32;

audio::Master::Master()
{
    std::cout << "Initializing audio master...\n";

    categories_.reserve(MAX_CATEGORIES);

    device_ = alcOpenDevice(nullptr);

    if (!device_)
    {
        throw std::runtime_error("(AL) Failed to open audio device");
    }

    // get device info
    const ALchar* info = alcGetString(device_, ALC_DEVICE_SPECIFIER);
    std::cout << "Opened audio device: " << (info ? info : "Unknown") << std::endl;

    context_ = alcCreateContext(device_, nullptr);

    if (!context_)
    {
        alcCloseDevice(device_);
        device_ = nullptr;
        throw std::runtime_error("(AL) Failed to create audio context");
    }

    std::cout << "Created audio context" << std::endl;

    if (!alcMakeContextCurrent(context_))
    {
        alcDestroyContext(context_);
        alcCloseDevice(device_);
        context_ = nullptr;
        device_ = nullptr;
        throw std::runtime_error("(AL) Failed to make audio context current");
    }

    std::cout << "Audio context is now current" << std::endl;

    //setup
    alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
}

void audio::Master::SetListenerOrientation(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up)
{
    alListener3f(AL_POSITION, position.x, position.y, position.z);
    // alListener3f(AL_VELOCITY, 0.0f, 0.0f, 0.0f);
    ALfloat orientation[] = {forward.x, forward.y, forward.z, up.x, up.y, up.z};
    alListenerfv(AL_ORIENTATION, orientation);
}

void audio::Master::SetListenerOrientation(const glm::mat4& world_matrix)
{
    glm::vec3 position = glm::vec3(world_matrix[3]);
    glm::vec3 forward = -glm::normalize(glm::vec3(world_matrix[2]));
    glm::vec3 up = glm::normalize(glm::vec3(world_matrix[1]));
    SetListenerOrientation(position, forward, up);
}

audio::Category* audio::Master::GetCategory(const std::string& name)
{
    auto it = category_map_.find(name);
    if (it != category_map_.end())
    {
        return it->second;
    }

    if (categories_.size() >= MAX_CATEGORIES)
    {
        throw std::runtime_error("(AL) Maximum number of audio categories reached");
    }

    Category* new_category = &categories_.emplace_back(name);
    category_map_[name] = new_category;

    AUDIO_DBG(std::cout << "Created new audio category: " << name << std::endl;)

    return new_category;
}

void audio::Master::SetMasterVolume(float volume)
{
    master_volume_ = volume;
    alListenerf(AL_GAIN, master_volume_);
    AUDIO_DBG(std::cout << "Set master volume to " << master_volume_ << std::endl;)
}

audio::Master::~Master()
{
    std::cout << "Shutting down audio master...\n";

    if (context_)
    {
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(context_);
        context_ = nullptr;
    }

    if (device_)
    {
        alcCloseDevice(device_);
        device_ = nullptr;
    }
}

audio::Category::Category(const std::string& name) : name_(name) {}

void audio::Category::SetVolume(float volume)
{
    volume_ = volume;
    for (Source* source = first_source_; source; source = source->cat_next_)
    {
        source->UpdateVolume();
    }
}
