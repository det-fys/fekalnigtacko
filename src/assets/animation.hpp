#pragma once

#include <memory>
#include <string>
#include <vector>

#include "utils/transform.hpp"

namespace assets
{
class Skeleton;

struct AnimationChannel
{
    int bone_index;
    const Transform* const* frames;
};

class Animation
{
public:
    Animation() = default;
    static std::shared_ptr<const Animation> LoadFromFile(const std::string& filename, const Skeleton* skeleton);

    size_t GetNumFrames() const { return num_frames_; }
    float GetTPS() const { return tps_; }
    float GetDuration() const { return duration_; }
    bool IsCyclic() const { return cyclic_; }

    size_t GetNumChannels() const { return channels_.size(); }
    const AnimationChannel& GetChannel(int index) const { return channels_[index]; }

private:
    size_t num_frames_ = 0;
    float tps_ = 24.0f;
    bool cyclic_ = false;
    float duration_ = 0.0f;

    std::vector<AnimationChannel> channels_;
    std::vector<const Transform*> frame_refs_;
    std::vector<Transform> frames_;
};

} // namespace assets