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

    ~Sound();

private:
    unsigned int buffer_ = 0;

    std::string category_name_;

    float volume_ = 1.0f;
    float pitch_ = 1.0f;
};

} // namespace audio