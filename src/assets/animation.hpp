#pragma once

#include <vector>
#include <memory>
#include <string>

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
		size_t num_frames_ = 0;
		float tps_ = 24.0f;

		std::vector<AnimationChannel> channels_;
		std::vector<const Transform*> frame_refs_;
		std::vector<Transform> frames_;

	public:
		Animation() = default;

		size_t GetNumFrames() const { return num_frames_; }
		float GetTPS() const { return tps_; }
		float GetDuration() const { return static_cast<float>(num_frames_) / tps_; }

		size_t GetNumChannels() const { return channels_.size(); }
		const AnimationChannel& GetChannel(int index) const { return channels_[index]; }
	
		static std::shared_ptr<const Animation> LoadFromFile(const std::string& filename, const Skeleton* skeleton);
		
	};


}