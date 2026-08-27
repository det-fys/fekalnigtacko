#pragma once

#include <string>

#include "gameview/modelview.hpp"
#include "mapgen/heightmesh.hpp"
#include "object.hpp"

namespace edit
{

using namespace game::view;

class StaticObject : public Object
{
public:
    using Super = Object;

    StaticObject(Project& project, const glm::mat4& trans, const std::string& model_name);

    virtual void Draw(const gfx::DrawContext& ctx) override;
    virtual void DrawOverlay(const DrawOverlayContext& ctx) override;

    virtual void SetTransform(const glm::mat4& trans) override;
    virtual void Clone(const glm::mat4& trans) override;
    virtual void Delete() override;

    const std::string& GetModelName() const { return model_name_; }
    const std::shared_ptr<const ModelView>& GetModel() const { return model_; }
    const std::shared_ptr<const mg::HeightMesh>& GetHeightMesh() const { return heightmesh_; }

private:
    void UpdateAABB();

    void MaybeInvalidateChunks();

private:
    std::string model_name_;
    std::shared_ptr<const ModelView> model_;
    std::shared_ptr<const mg::HeightMesh> heightmesh_;
};

} // namespace edit
