#include "skeleton.hpp"

#include "utils/files.hpp"
#include <sstream>
#include <stdexcept>

void assets::Skeleton::AddBone(const std::string& name, const std::string& parent_name, const Transform& transform)
{
	int index = static_cast<int>(bones_.size());

	Bone& bone = bones_.emplace_back();
	bone.name = name;
	bone.parent_idx = GetBoneIndex(parent_name);
	bone.bind_transform = transform;
	bone.inv_bind_matrix = glm::inverse(transform.ToMatrix());

	bone_map_[bone.name] = index;
}

int assets::Skeleton::GetBoneIndex(const std::string& name) const
{
	auto it = bone_map_.find(name);
	if (it != bone_map_.end()) {
		return it->second;
	}

	return -1;
}

const assets::Animation* assets::Skeleton::GetAnimation(const std::string& name) const
{
	auto it = anims_.find(name);
	if (it != anims_.end()) {
		return it->second.get();
	}
	return nullptr;
}

std::shared_ptr<const assets::Skeleton> assets::Skeleton::LoadFromFile(const std::string& filename)
{
	std::istringstream ifs = fs::ReadFileAsStream(filename);

	std::shared_ptr<Skeleton> skeleton = std::make_shared<Skeleton>();

	std::string line;

	while (std::getline(ifs, line))
	{
		if (line.empty() || line[0] == '#') // Skip empty lines and comments
			continue;

		std::istringstream iss(line);

		std::string command;
		iss >> command;

		if (command == "b")
		{
			Transform t;
			glm::vec3 angles;
			std::string bone_name, parent_name;
		
			iss >> bone_name >> parent_name;
			iss >> t.position.x >> t.position.y >> t.position.z;
			iss >> angles.x >> angles.y >> angles.z;
			iss >> t.scale;

			if (iss.fail())
			{
				throw std::runtime_error("Failed to parse bone definition in file: " + filename);
			}

			t.SetAngles(angles);

			skeleton->AddBone(bone_name, parent_name, t);
		}
		else if (command == "anim")
		{
			std::string anim_name, anim_filename;
			iss >> anim_name >> anim_filename;

			if (iss.fail())
			{
				throw std::runtime_error("Failed to parse animation definition in file: " + filename);
			}

			std::shared_ptr<const Animation> anim = Animation::LoadFromFile("data/" + anim_filename + ".anim", skeleton.get());
			skeleton->AddAnimation(anim_name, anim);
		}

		
	}

	return skeleton;
}
