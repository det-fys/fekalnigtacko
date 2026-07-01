#pragma once

#include "assets/model.hpp"
#include "gfx/surface.hpp"

namespace game::view
{

class ModelView : public assets::Asset
{
public:
    ModelView(std::shared_ptr<const assets::Model> model);
    
    static std::shared_ptr<ModelView> Load(const std::string& name);

    const std::shared_ptr<const assets::Model>& GetModel() const { return model_; }

    std::span<const gfx::Surface> GetSurfaces() const { return surfaces_; }

private:
    void CreateVA();
    void CreateSurfaces();

private:
    std::shared_ptr<const assets::Model> model_;

    gfx::MeshFlags mflags_;
    std::shared_ptr<const gfx::VertexArray> va_;
    std::vector<gfx::Surface> surfaces_;

};

}