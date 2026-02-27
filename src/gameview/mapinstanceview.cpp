#include "mapinstanceview.hpp"
#include "assets/cache.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

game::view::MapInstanceView::MapInstanceView(const std::string& map_name) 
{
    map_ = assets::CacheManager::GetMap("data/" + map_name + ".map");

    objs_visible_.resize(map_->GetStaticObjects().size(), true);
}

void game::view::MapInstanceView::Draw(const game::view::DrawArgs& args) const
{
    const auto& basemodel = map_->GetBaseModel();

    if (!basemodel || !basemodel->GetMesh())
        return;

    const auto& mesh = *basemodel->GetMesh();

    const float max_dist = args.render_distance + 200.0f;
    const float max_dist2 = max_dist * max_dist;

    for (const auto& chunks = map_->GetChunks(); const auto& chunk : chunks)
    {
        glm::vec3 center = (chunk.aabb.min + chunk.aabb.max) * 0.5f;
        if (glm::distance2(args.eye, center) > max_dist2)
            continue;

        if (!args.frustum.IsAABBVisible(chunk.aabb))
            continue;

        DrawChunk(args, mesh, chunk);
    }

}

void game::view::MapInstanceView::EnableObj(net::ObjNum num, bool enable)
{
    size_t i = static_cast<size_t>(num);
    if (i >= objs_visible_.size())
        return;

    objs_visible_[i] = enable;
}

void game::view::MapInstanceView::DrawChunk(const game::view::DrawArgs& args, const assets::Mesh& basemesh,
                                            const assets::Chunk& chunk) const 
{
    for (const auto& surface_range : chunk.surfaces)
    {
        auto& surface = basemesh.surfaces[surface_range.idx];

        gfx::DrawSurfaceCmd cmd;
        cmd.surface = &surface;
        cmd.first = surface_range.first;
        cmd.count = surface_range.count;
        args.dlist.AddSurface(cmd);
    }

    const auto& objs = map_->GetStaticObjects();

    for (size_t i = 0; i < chunk.num_objs; ++i)
    {
        size_t abs_i = chunk.first_obj + i;

        if (abs_i >= objs.size())
            continue;

        if (!objs_visible_[abs_i])
            continue;

        const auto& obj = objs[abs_i];

        if (!obj.model || !obj.model->GetMesh())
            continue;

        if (!args.frustum.IsAABBVisible(obj.aabb))
            continue;

        const auto& surfaces = obj.model->GetMesh()->surfaces;

        for (const auto& surface : surfaces)
        {
            gfx::DrawSurfaceCmd cmd;
            cmd.surface = &surface;
            cmd.matrices = &obj.node.matrix;
            // cmd.color_mod = glm::vec4(obj.color, 1.0f);
            args.dlist.AddSurface(cmd);
        }
    }
}
