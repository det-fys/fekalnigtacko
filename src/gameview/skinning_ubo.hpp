#pragma once

#include "gfx/uniform_buffer.hpp"
#include "game/skeletoninstance.hpp"

namespace game::view
{

class SkinningUBO : public gfx::UniformBuffer<glm::mat4>
{
public:
    SkinningUBO(const SkeletonInstance& sk);
    void Update();

private:
    const SkeletonInstance& sk_;

};



}