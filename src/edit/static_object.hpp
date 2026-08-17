#pragma once

#include <string>

#include "object.hpp"
#include "gameview/modelview.hpp"

namespace edit
{

using namespace game::view;

class StaticObject : public Object
{
public:
    StaticObject(const std::string& model_name);
    StaticObject(std::shared_ptr<const ModelView> model);

    virtual void Draw(const gfx::DrawContext& ctx) override;
    virtual void DrawOverlay(const DrawOverlayContext& ctx) override;

private:
    std::shared_ptr<const ModelView> model_;

};

}