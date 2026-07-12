#pragma once

#include "id.hpp"
#include "deform_grid_info.hpp"

#include <glm/glm.hpp>

namespace gfx
{

using DeformTextureID = ID;

struct DeformTextureDescriptor
{
    DeformGridInfo grid{};
};




}