#include "deform_grid.hpp"

game::DeformGrid::DeformGrid(const gfx::DeformGridInfo& info)
    : info_(info), data_(info_.res.x * info_.res.y * info_.res.z, glm::i8vec3(0))
{    

}

void game::DeformGrid::ApplyImpulse(const glm::vec3& pos, const glm::vec3& impulse, float radius)
{
    glm::vec3 step = (info_.max - info_.min) / glm::vec3(info_.res);

    glm::ivec3 ipos;

    // TODO: make more efficient
    for (ipos.z = 0; ipos.z < info_.res.z; ++ipos.z)
    {
        for (ipos.y = 0; ipos.y < info_.res.y; ++ipos.y)
        {
            for (ipos.x = 0; ipos.x < info_.res.x; ++ipos.x)
            {
                glm::vec3 texel_pos = info_.min + glm::vec3(ipos) * step;

                if (glm::distance2(pos, texel_pos) < radius * radius)
                {
                    size_t idx = GetTexelIndex(ipos);
                    glm::vec3 offset = UnpackOffset(data_[idx]);
                    offset += impulse;
                    data_[idx] = PackOffset(offset);
                }
            }
        }
    }


}

glm::i8vec3 game::DeformGrid::PackOffset(const glm::vec3& offset)
{
    return glm::i8vec3(glm::round(glm::clamp(offset / info_.max_offset, -1.0f, 1.0f) * 127.0f));
}

glm::vec3 game::DeformGrid::UnpackOffset(const glm::i8vec3& packed)
{
    return glm::clamp(glm::vec3(packed) * 0.0078740157480315f, -1.0f, 1.0f) * info_.max_offset;
}
