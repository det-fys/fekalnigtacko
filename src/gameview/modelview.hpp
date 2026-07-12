#pragma once

#include <optional>

#include "assets/model.hpp"
#include "gfx/mesh.hpp"
#include "gfx/material.hpp"
#include "gfx/scene.hpp"
#include "utils/defs.hpp"

namespace game::view
{

struct ModelViewSurface
{
    std::shared_ptr<gfx::Material> material;
    uint32_t tri_offset;
    uint32_t tri_count;
};

class ModelView : public assets::Asset
{
public:
    ModelView(std::shared_ptr<const assets::Model> model);
    DELETE_COPY_MOVE(ModelView);

    static std::shared_ptr<ModelView> Load(const std::string& name);

    const std::shared_ptr<const assets::Model>& GetModel() const { return model_; }

    const gfx::Mesh& GetMesh() const { return *mesh_; }
    std::span<const ModelViewSurface> GetSurfaces() const { return surfaces_; }

    void Draw(const gfx::DrawContext& ctx, const glm::mat4& matrix, std::span<const glm::vec4> colors,
              gfx::SkeletonPoseID pose_id = 0, gfx::DeformTextureID deform_id = 0,
              uint32_t surface_mask = 0xFFFFFFFF) const;

private:
    void CreateMesh();
    void CreateSurfaces();

private:
    std::shared_ptr<const assets::Model> model_;

    std::optional<gfx::Mesh> mesh_;
    std::vector<ModelViewSurface> surfaces_;

};

}