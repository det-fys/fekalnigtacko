#pragma once

#include <string>

#include "gameview/modelview.hpp"
#include "mapgen/resources.hpp"
#include "object.hpp"

namespace edit
{

using namespace game::view;

class StaticObject : public Object
{
public:
    using Super = Object;

    StaticObject(Project& project, const FarMatrix& trans, uint32_t model_id);

    virtual void Draw(const gfx::DrawContext& ctx) override;
    virtual void DrawOverlay(const DrawOverlayContext& ctx) override;

    virtual void SetTransform(const FarMatrix& trans) override;
    virtual void Clone(const FarMatrix& trans) override;
    virtual void Delete() override;

    uint32_t GetModelID() const { return model_id_; }
    const std::shared_ptr<const ModelView>& GetModel() const { return model_; }

private:
    void UpdateAABB();

    void MaybeInvalidateChunks();

private:
    uint32_t model_id_ = mg::INVALID_RES_ID;
    std::shared_ptr<const ModelView> model_;

};

} // namespace edit
