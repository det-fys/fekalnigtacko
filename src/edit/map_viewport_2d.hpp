#pragma once

#include "im/viewport_2d.hpp"
#include "map_edit_context.hpp"
#include "im/scene_view.hpp"

namespace edit
{

class MapViewport2D : public im::Viewport2D
{
public:
    using Super = im::Viewport2D;

    MapViewport2D(MapEditContext& context);

protected:
    virtual void Draw(ImDrawList& draw_list) override;

private:
    MapEditContext& context_;

    im::SceneView scene_view_;
};

} // namespace edit
