#pragma once

#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>

#include "utils/files.hpp"
#include "utils/transform.hpp"
#include "utils/cmdlinestream.hpp"

namespace assets
{

using CmdCallback = std::function<bool(const std::string& command, CmdLineStream& iss)>;
using CmdCallbackVoid = std::function<void(const std::string& command, CmdLineStream& iss)>;

void LoadCMDStream(std::istream& is, CmdCallback handler);
void LoadCMDFile(const std::string& filename, CmdCallbackVoid handler);

inline void ParseTransform(CmdLineStream& iss, Transform& trans)
{
    iss >> trans.position.x >> trans.position.y >> trans.position.z;
    iss >> trans.rotation.x >> trans.rotation.y >> trans.rotation.z >> trans.rotation.w;
    iss >> trans.scale;
}

std::string ParseString(CmdLineStream& iss);

} // namespace assets