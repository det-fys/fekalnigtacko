#pragma once

#include "utils/files.hpp"
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>

namespace assets
{

void LoadCMDFile(const std::string& filename,
                 const std::function<void(const std::string& command, std::istringstream& iss)>& handler);

} // namespace assets