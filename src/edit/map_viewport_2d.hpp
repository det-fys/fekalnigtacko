#pragma once

#include "map_viewport.hpp"
#include "im/viewport_2d_controls.hpp"
#include "utils/aabb.hpp"

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

    void DrawChunks(ImDrawList& draw_list);
    void DrawChunk(ImDrawList& draw_list, const glm::ivec2& coord);

private:
    im::Viewport2DControls controls_;
    glm::vec2 new_obj_pos_{0.0f};

    // temp
    AABB2 aabb_{};
};

} // namespace edit
