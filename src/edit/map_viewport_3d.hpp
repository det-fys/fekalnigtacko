#pragma once

#include "im/viewport.hpp"
#include "map_edit_context.hpp"
#include "im/scene_view.hpp"

namespace edit
{

class MapViewport3D : public im::Viewport
{
public:
    using Super = im::Viewport;

    MapViewport3D(MapEditContext& context);

protected:
    virtual void Draw(ImDrawList& draw_list) override;

private:
    MapEditContext& context_;

    im::SceneView scene_view_;
};

} // namespace edit
