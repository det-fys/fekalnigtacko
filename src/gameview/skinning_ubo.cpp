#include "skinning_ubo.hpp"
#include "gfx/shader_defs.hpp"

game::view::SkinningUBO::SkinningUBO(const SkeletonInstance& sk) : sk_(sk)
{
}

void game::view::SkinningUBO::Update()
{
    static glm::mat4 skin_mats[SD_MAX_BONES];

    const auto& skeleton = sk_.GetSkeleton();
    size_t num_mats = std::min(skeleton->GetNumBones(), static_cast<size_t>(SD_MAX_BONES));

    for (size_t i = 0; i < num_mats; ++i)
    {
        const auto& bone = skeleton->GetBone(i);
        const TransformNode& node = sk_.GetBoneNode(i);
        skin_mats[i] = node.matrix * bone.inv_bind_matrix;
    }

    SetData(skin_mats, num_mats);
}
