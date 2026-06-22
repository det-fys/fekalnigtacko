#pragma once

#include "master.hpp"

#include <memory>

namespace audio
{

class Sound
{
public:
    Sound();
    static std::shared_ptr<const Sound> LoadFromFile(const std::string& path);

    unsigned int GetBufferId() const { return buffer_; }

    const std::string& GetCategoryName() const { return category_name_; }

    float GetVolume() const { return volume_; }
    float GetPitch() const { return pitch_; }

    float GetRefDistance() const { return ref_distance_; }
    float GetRolloffFactor() const { return rolloff_ractor_; }
    float GetMaxDistance() const { return max_distance_; }

    ~Sound();

private:
    unsigned int buffer_ = 0;

    std::string category_name_;

    float volume_ = 1.0f;
    float pitch_ = 1.0f;

    float ref_distance_ = 1.0f;
    float rolloff_ractor_= 1.0f;
    float max_distance_ = 200.0f;
};

} // namespace audio