#include "animation.hpp"
#include "skeleton.hpp"

#include "utils/files.hpp"
#include <sstream>
#include <stdexcept>

std::shared_ptr<const assets::Animation> assets::Animation::LoadFromFile(const std::string& filename, const Skeleton* skeleton)
{
	std::istringstream ifs = fs::ReadFileAsStream(filename);
	
	std::shared_ptr<Animation> anim = std::make_shared<Animation>();

	int last_frame = 0;
	std::vector<size_t> frame_indices;

	auto FillFrameRefs = [&](int end_frame)
		{
			int channel_start = ((int)anim->channels_.size() - 1) * (int)anim->num_frames_;
			int target_size = channel_start + end_frame;
			size_t num_frame_refs = frame_indices.size();

			if (num_frame_refs >= target_size)
			{
				return; // Already filled
			}

			if (num_frame_refs % anim->num_frames_ == 0)
			{
				throw std::runtime_error("Cannot fill frames of channel that has 0 frames: " + filename);
			}

			size_t last_frame_idx = frame_indices.back();
			frame_indices.resize(target_size, last_frame_idx);
		};

	std::string line;

	while (std::getline(ifs, line))
	{
		if (line.empty() || line[0] == '#') // Skip empty lines and comments
			continue;

		std::istringstream iss(line);

		std::string command;
		iss >> command;

		if (command == "f")
		{
			if (anim->num_frames_ == 0)
			{
				throw std::runtime_error("Frame data specified before number of frames in animation file: " + filename);
			}

			if (anim->channels_.empty())
			{
				throw std::runtime_error("Frame data specified before any channels in animation file: " + filename);
			}

			int frame_index;
			iss >> frame_index;

			if (frame_index < 0 || frame_index >= (int)anim->num_frames_)
			{
				throw std::runtime_error("Frame index out of bounds in animation file: " + filename);
			}

			if (frame_index < last_frame)
			{
				throw std::runtime_error("Frame indices must be in ascending order in animation file: " + filename);
			}

			last_frame = frame_index;

			Transform t;
			iss >> t.position.x >> t.position.y >> t.position.z;
			glm::vec3 angles_deg;
			iss >> angles_deg.x >> angles_deg.y >> angles_deg.z;
			t.SetAngles(angles_deg);
			iss >> t.scale;

			size_t idx = anim->frames_.size();
			anim->frames_.push_back(t);
			
			FillFrameRefs(frame_index); // Fill to current frame
			frame_indices.push_back(idx);
		}
		else if (command == "ch")
		{
			std::string name;
			iss >> name;

			int bone_index = skeleton->GetBoneIndex(name);

			if (bone_index < 0)
			{
				throw std::runtime_error("Bone referenced in animation not found in provided skeleton: " + name);
			}

			FillFrameRefs(anim->num_frames_); // Fill to end for last channel

			AnimationChannel& channel = anim->channels_.emplace_back();
			channel.bone_index = bone_index;
			channel.frames = nullptr; // Will be set up later

			last_frame = 0;

		}
		else if (command == "frames")
		{
			iss >> anim->num_frames_;
		}
		else if (command == "fps")
		{
			iss >> anim->tps_;
		}
	}
	
	if (anim->channels_.empty())
	{
		throw std::runtime_error("No channels found in animation file: " + filename);
	}

	FillFrameRefs(anim->num_frames_); // Fill to end for last channel

	// Set up frame pointers
	anim->frame_refs_.resize(frame_indices.size());
	for (size_t i = 0; i < frame_indices.size(); ++i)
	{
		anim->frame_refs_[i] = &anim->frames_[frame_indices[i]];
	}

	// Set up channel frame pointers
	for (size_t i = 0; i < anim->channels_.size(); ++i)
	{
		AnimationChannel& channel = anim->channels_[i];
		channel.frames = &anim->frame_refs_[i * anim->num_frames_];
	}

	return anim;
}
