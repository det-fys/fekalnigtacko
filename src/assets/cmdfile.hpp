#pragma once

#include "utils/files.hpp"
#include "utils/transform.hpp"
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>

namespace assets
{

void LoadCMDFile(const std::string& filename,
                 const std::function<void(const std::string& command, std::istringstream& iss)>& handler);


inline void ParseTransform(std::istringstream& iss, Transform& trans)
{
    iss >> trans.position.x >> trans.position.y >> trans.position.z;
    iss >> trans.rotation.x >> trans.rotation.y >> trans.rotation.z >> trans.rotation.w;
    iss >> trans.scale;
}

std::string ParseString(std::istringstream& iss);

} // namespace assets