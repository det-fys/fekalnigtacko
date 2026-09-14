#pragma once

#include <string>

#include "gfx/material_desc.hpp"
#include "cmdfile.hpp"

namespace assets
{

struct ModelMaterial
{
    std::string name;

    std::string texture_name;
    gfx::MaterialProperties properties;
};

ModelMaterial ParseMaterial(CmdLineStream& iss);

} // namespace assets
