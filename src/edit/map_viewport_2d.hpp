#pragma once

#include "map_viewport.hpp"
#include "im/viewport_2d_controls.hpp"

namespace edit
{

class MapViewport2D : public MapViewport
{
public:
    using Super = MapViewport;

    MapViewport2D(MapEditContext& context);

protected:
    virtual void Update();
    virtual void Draw(ImDrawList& draw_list) override;

private:
    void ShowContextMenu();

private:
    im::Viewport2DControls controls_;
    glm::vec2 new_obj_pos_{0.0f};
};

} // namespace edit
