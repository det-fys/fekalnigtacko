#pragma once

#include "id.hpp"

namespace gfx
{
    
using SkeletonPoseID = ID;

constexpr size_t MAX_BONES = 128;

struct SkeletonPoseDescriptor
{
    size_t num_bones = 0;
};

}
