#pragma once

#include "assets/map.hpp"
#include "draw_args.hpp"
#include "net/defs.hpp"

namespace game::view
{

class MapInstanceView
{
public:
    MapInstanceView(const std::string& map_name);

    void LoadNext();
    bool IsLoaded() const { return loader_.get() == nullptr; }
    int GetLoadingPercent() const;

    void Draw(const game::view::DrawArgs& args) const;

    void EnableObj(net::ObjNum num, bool enable);

private:
    void DrawChunk(const game::view::DrawArgs& args, const assets::Mesh& basemesh, const assets::Chunk& chunk) const;

private:
    std::unique_ptr<assets::MapLoader> loader_;
    std::shared_ptr<const assets::Map> map_;
    std::vector<bool> objs_visible_;

};



}