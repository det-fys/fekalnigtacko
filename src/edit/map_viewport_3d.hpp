#pragma once

#include "map_viewport.hpp"

namespace edit
{

class MapViewport3D : public MapViewport
{
public:
    using Super = MapViewport;

    MapViewport3D(MapEditContext& context);

protected:
    virtual void Update() override;
    virtual void Draw(ImDrawList& draw_list) override;

};

} // namespace edit
