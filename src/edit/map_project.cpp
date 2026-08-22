#define GLM_ENABLE_EXPERIMENTAL

#include "map_project.hpp"

#include <algorithm>
#include <set>
#include <tuple>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include "assets/asset_manager.hpp"
#include "im/utils.hpp"
#include "map_viewport.hpp"
#include "object.hpp"
#include "static_object.hpp"

edit::Project::Project() : static_models_root_("root")
{
    InitStaticModels();

    SetupChunks(16);
}

void edit::Project::Update()
{
    UpdateChunks();
}

void edit::Project::Draw(const gfx::DrawContext& ctx)
{
    for (auto& obj : all_objs_)
    {
        if (!ctx.frustum.IsSphereVisible({obj->GetPosition(), 5.0f}))
            continue;

        obj->Draw(ctx);
    }

    static glm::mat4 identity(1.0f);

    for (auto& [chunk_pos, chunk] : chunks_)
    {
        if (!ctx.frustum.IsAABBVisible(chunk.mgchunk.aabb))
            continue;

        if (chunk.model)
            chunk.model->Draw(ctx, identity, {});
    }

    world_env_.SetDayTime(daytime_);

    if (draw_world_env_)
        world_env_.Draw(ctx);
}

gfx::Environment edit::Project::GetSceneEnvironment()
{
    // gfx::Environment env{};
    // env.clear_color = glm::vec3(0.3f);
    // env.ambient_light = glm::vec3(1.0f);
    // return env;

    return world_env_.GetEnv();
}

float edit::Project::GetMapChunkSize()
{
    return map_config_.chunk_size_m;
}

void edit::Project::DrawOverlay(const DrawOverlayContext& ctx)
{
    for (auto& obj : all_objs_)
    {
        if (!ctx.viewport.GetFrustum().IsSphereVisible({obj->GetPosition(), 5.0f}))
            continue;

        obj->DrawOverlay(ctx);
    }
}

void edit::Project::AddStaticObject(const glm::vec2& pos, const std::string& model_name)
{
    ClearSelection(); // to avoid invalid pointers
    auto& obj = static_objs_.emplace_back(*this, model_name);
    obj.SetTransform(glm::translate(glm::mat4(1.0f), glm::vec3(pos, 0.0f))); // TODO: Z from heightmap
    NewObjectAdded(obj);
}

void edit::Project::MakeSelection2D(const glm::vec2& min, const glm::vec2* max)
{
    // find nearest obj
    float nearest_dist2 = std::numeric_limits<float>().max();
    Object* nearest_obj = nullptr;

    for (auto& obj : all_objs_)
    {
        auto pos2d = glm::vec2(obj->GetPosition());
        auto d = pos2d - min;
        auto dist2 = glm::dot(d, d);
        
        if (dist2 > nearest_dist2)
            continue;

        nearest_obj = obj;
        nearest_dist2 = dist2;
    }

    if (!nearest_obj)
        return;

    if (nearest_obj->IsSelected())
        Deselect(*nearest_obj);
    else
        Select(*nearest_obj);
}

bool edit::Project::HasSelection() const
{
    return !selection_.empty();
}

const glm::mat4* edit::Project::GetSelectionMatrix() const
{
    if (!HasSelection())
        return nullptr;

    return &selection_.front().obj->GetTransform();
}

void edit::Project::ApplySelectionTransform(const glm::mat4& delta)
{
    if (!HasSelection())
        return;

    auto& first_entry = selection_.front();
    auto& first_obj = *first_entry.obj;
    first_obj.SetTransform(delta * first_obj.GetTransform());

    const auto& first_trans = first_obj.GetTransform();

    for (uint32_t i = 1; i < selection_.size(); ++i)
    {
        auto& entry = selection_[i];
        auto& obj = entry.obj;
        obj->SetTransform(entry.offset * first_trans);
    }
}

void edit::Project::ClearSelection()
{
    for (auto& entry : selection_)
    {
        entry.obj->SetSelected(false);
    }

    selection_.clear();
}

void edit::Project::DeleteSelection()
{
    static_objs_.erase(std::remove_if(static_objs_.begin(), static_objs_.end(),
                                      [](const StaticObject& obj) { return obj.IsSelected(); }),
                       static_objs_.end());

    all_objs_.erase(std::remove_if(all_objs_.begin(), all_objs_.end(), [](Object* obj) { return obj->IsSelected(); }),
                    all_objs_.end());

    selection_.clear();
}

void edit::Project::InvalidateChunk(const glm::ivec2& chunk_pos)
{
    invalid_chunks_.insert(chunk_pos);
    DelayChunkUpdates();
}

void edit::Project::DelayChunkUpdates()
{
    chunk_update_time_ = ImGui::GetTime() + 0.1f;
}

void edit::Project::InitStaticModels()
{
    auto& root = static_models_root_;

    auto& buildings = root.children.emplace_back("buildings");
    buildings.children.emplace_back("house1", "house1");
    buildings.children.emplace_back("house2", "house2");
    buildings.children.emplace_back("rabbithouse1", "rabbithouse1");
    buildings.children.emplace_back("tuning_house", "tuning_house");

    auto& natural = root.children.emplace_back("natural");
    natural.children.emplace_back("bush1", "bush1");
    natural.children.emplace_back("bush2", "bush2");
    natural.children.emplace_back("biggertree", "biggertree");
    natural.children.emplace_back("commontree", "commontree");
    natural.children.emplace_back("pine", "pine");
    natural.children.emplace_back("spruce", "spruce");
}

void edit::Project::Select(Object& obj)
{
    if (obj.IsSelected())
        return;

    obj.SetSelected(true);

    auto selection_matrix = GetSelectionMatrix();
    auto offset = selection_matrix ? glm::inverse(*selection_matrix) * obj.GetTransform() : glm::mat4(1.0f);

    selection_.emplace_back(&obj, offset);
}

void edit::Project::Deselect(Object& obj)
{
    if (!obj.IsSelected())
        return;

    obj.SetSelected(false);

    FixSelectionList();
}

void edit::Project::FixSelectionList()
{
    selection_.erase(std::remove_if(selection_.begin(), selection_.end(),
                                    [](const SelectionListEntry& entry) { return !entry.obj->IsSelected(); }),
                     selection_.end());
}

void edit::Project::NewObjectAdded(Object& obj)
{
    UpdateAllObjectsList();

    // select the new object
    Select(obj);
}

void edit::Project::UpdateAllObjectsList()
{
    all_objs_.clear();

    for (auto& obj : static_objs_)
    {
        all_objs_.emplace_back(&obj);
    }
}

void edit::Project::SetupChunks(uint32_t size)
{
    map_config_.chunks = size;
    map_config_.chunk_size_m = 128.0f;
    map_config_.chunk_tiles = 256;

    chunks_.clear();

    int half_size = static_cast<int>(size) / 2;

    for (int y = -half_size; y < half_size; ++y)
    {
        for (int x = -half_size; x < half_size; ++x)
        {
            glm::ivec2 chunk_pos(x, y);
            InvalidateChunk(chunk_pos);
        }
    }
}

void edit::Project::UpdateChunks()
{
    if (invalid_chunks_.empty())
        return;

    if (chunk_update_time_ > ImGui::GetTime())
        return;

    // pick one chunk to update
    auto chunk_pos = *invalid_chunks_.begin();
    invalid_chunks_.erase(invalid_chunks_.begin());
    UpdateChunk(chunk_pos);

}

void edit::Project::UpdateChunk(const glm::ivec2& chunk_pos)
{
    AABB2 chunk_aabb = mg::GetChunkAABB(map_config_, chunk_pos, true);

    std::vector<mg::ChunkStaticObject> objs;
    
    // collect relevant objs
    for (const auto& obj : static_objs_)
    {
        //auto pos2d = glm::vec2(obj.GetPosition());
        const auto& obj_aabb = obj.GetAABB();
        AABB2 obj_aabb_2d(glm::vec2(obj_aabb.min), glm::vec2(obj_aabb.max));

        if (!chunk_aabb.CollidesWith(obj_aabb_2d))
            continue;

        auto& mgobj = objs.emplace_back();
        mgobj.trans = obj.GetTransform();
        mgobj.model_name = obj.GetModelName();
        mgobj.heightmesh = obj.GetHeightMesh();
    }

    mg::ChunkParams params{};
    params.coord = chunk_pos;
    params.objs = objs;

    Chunk& chunk = chunks_[chunk_pos];
    chunk.mgchunk = mg::GenerateChunk(map_config_, params);
    
    chunk.vis_verts.clear();
    chunk.vis_edges.clear();
    chunk.vis_tris.clear();

    // generate verts & edges for vis
    for (const auto& vert : chunk.mgchunk.mesh.verts)
    {
        chunk.vis_verts.push_back(vert.pos);
    }

    std::set<std::tuple<uint32_t, uint32_t>> edges_set;
    for (const auto& tri : chunk.mgchunk.mesh.tris)
    {
        chunk.vis_tris.emplace_back(tri[0], tri[1], tri[2]);

        edges_set.emplace(std::min(tri[0], tri[1]), std::max(tri[0], tri[1]));
        edges_set.emplace(std::min(tri[1], tri[2]), std::max(tri[1], tri[2]));
        edges_set.emplace(std::min(tri[2], tri[0]), std::max(tri[2], tri[0]));
    }

    for (const auto& edge : edges_set)
    {
        chunk.vis_edges.push_back(edge);
    }

    assets::ModelDescriptor model_desc{};
    //model_desc.make_triangle_mesh = true;

    for (const auto& vert : chunk.mgchunk.mesh.verts)
    {
        model_desc.verts.positions.push_back(vert.pos);
        model_desc.verts.normals.push_back(vert.normal);
        model_desc.verts.uvs.push_back(vert.uv);
    }

    for (const auto& tri : chunk.mgchunk.mesh.tris)
    {
        model_desc.tris.emplace_back(tri[0], tri[1], tri[2]);
    }

    auto& surface = model_desc.surfaces.emplace_back();
    surface.name = "grass";
    surface.texture_name = "grass";
    surface.tri_offset = 0;
    surface.tri_count = static_cast<uint32_t>(model_desc.tris.size());

    auto model = std::make_shared<assets::Model>(std::move(model_desc));
    chunk.model = std::make_shared<ModelView>(model);
}
