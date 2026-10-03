#include "map_project.hpp"

#include <algorithm>
#include <set>
#include <tuple>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include "assets/asset_manager.hpp"
#include "im/utils.hpp"
#include "gameview/chunk_manager_client.hpp"
#include "map_viewport.hpp"
#include "object.hpp"
#include "static_object.hpp"

edit::Project::Project(MapEditProperties& properties) : properties_(properties), static_models_root_("root")
{
    mg_res_ = assets::AssetManager::GetInstance().Get<mg::ResourceSet>("resources");
    InitStaticModels();

    SetupChunks();
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

    if (chunk_manager_)
        chunk_manager_->Draw(ctx);

    world_env_.SetDayTime(properties_.day_time);

    if (properties_.draw_world_env)
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

float edit::Project::GetTerrainHeight(const glm::vec2& pos) const
{
    if (!height_sampler_)
        return 0.0f;

    return height_sampler_->Get(pos);
}

void edit::Project::AddStaticObject(const glm::mat4& trans, const std::string& model_name)
{
    ClearSelection(); // to avoid invalid pointers
    auto& obj = static_objs_.emplace_back(*this, trans, model_name);
    NewObjectAdded(obj);
}

void edit::Project::AddStaticObject(const glm::vec2& pos, const std::string& model_name)
{
    AddStaticObject(GetNewObjectTransform(pos), model_name);
}

edit::Waypoint* edit::Project::AddWaypoint(const glm::mat4& trans)
{
    ClearSelection(); // to avoid invalid pointers
    auto id = utils::AllocNum(waypoints_, last_waypoint_id_);
    if (!id)
    {
        return nullptr; // max waypoints reached - should never happen
    }

    auto& wp = waypoints_.emplace(id, Waypoint(*this, trans, id)).first->second;
    NewObjectAdded(wp);
    return &wp;
}

edit::Waypoint* edit::Project::AddWaypoint(const glm::vec2& pos)
{
    return AddWaypoint(GetNewObjectTransform(pos));
}

edit::Waypoint* edit::Project::GetWaypoint(WaypointID id)
{
    if (id == 0)
        return nullptr;

    auto it = waypoints_.find(id);
    return (it != waypoints_.end()) ? &it->second : nullptr;
}

void edit::Project::SetHover(const glm::vec3& start, const glm::vec3& end)
{
    Object* hovered_obj = ObjectRaycast(start, end);

    for (auto& obj : all_objs_)
    {
        obj->SetHovered(obj == hovered_obj);
    }
}

void edit::Project::MakeSelection(const glm::vec3& start, const glm::vec3& end, bool additive)
{
    auto obj = ObjectRaycast(start, end);
    SelectOrDeselect({&obj, static_cast<size_t>(obj ? 1 : 0)}, additive, true);
}

void edit::Project::MakeSelection2D(const glm::vec2& min, const glm::vec2* max, bool additive)
{
    MakeSelection(glm::vec3(min, 1000.0f), glm::vec3(min, 1000.0f), additive);
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

    auto first_trans = delta * first_obj.GetTransform();
    first_obj.SetTransform(SnapTransform(first_trans));

    for (uint32_t i = 1; i < selection_.size(); ++i)
    {
        auto& entry = selection_[i];
        auto& obj = entry.obj;
        obj->SetTransform(SnapTransform(first_trans * entry.offset));
    }
}

void edit::Project::CloneSelection(const glm::mat4& trans)
{
    if (!HasSelection())
        return;

    // currently can only clone a single obj due to selection invalidation issues
    selection_[0].obj->Clone(trans);
}

void edit::Project::CloneSelection(const glm::vec2& pos)
{
    CloneSelection(GetNewObjectTransform(pos));
}

void edit::Project::LinkSelection(bool link)
{
    if (selection_.size() < 2)
        return;

    selection_[0].obj->Link(*selection_[1].obj, link);
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
    // notify objects that they are being deleted
    for (auto obj : all_objs_)
    {
        if (obj->IsSelected())
            obj->Delete();
    }

    // remove selected static objects
    static_objs_.erase(std::remove_if(static_objs_.begin(), static_objs_.end(),
                                      [](const StaticObject& obj) { return obj.IsSelected(); }),
                       static_objs_.end());

    // remove selected waypoints
    for (auto it = waypoints_.begin(); it != waypoints_.end();)
    {
        if (it->second.IsSelected())
            it = waypoints_.erase(it);
        else
            ++it;
    }

    // clear selection list
    selection_.clear();

    UpdateAllObjectsList();
}

void edit::Project::InvalidateChunk(const glm::ivec2& chunk_pos)
{
    if (chunk_manager_)
        chunk_manager_->InvalidateChunk(chunk_pos);
}

void edit::Project::GetChunkParams(mg::ChunkParams& params)
{
    AABB2 chunk_aabb = mg::GetChunkAABB(map_config_, params.coord, true);

    // collect relevant objs
    for (const auto& obj : static_objs_)
    {
        // auto pos2d = glm::vec2(obj.GetPosition());
        const auto& obj_aabb = obj.GetAABB();
        AABB2 obj_aabb_2d(glm::vec2(obj_aabb.min), glm::vec2(obj_aabb.max));

        if (!chunk_aabb.CollidesWith(obj_aabb_2d))
            continue;

        auto& mgobj = params.objs.emplace_back();
        mgobj.trans = obj.GetTransform();
        mgobj.model_name = obj.GetModelName();
        mgobj.heightmesh = obj.GetHeightMesh();
    }
    
    // collect splines
    for (const auto& [id, waypoint] : waypoints_)
    {
        mg::ChunkSplineNode node{};
        node.pos = waypoint.GetPosition();
        
        uint32_t link_count = 0;
        for (uint32_t i = 0; i < 4; ++i)
        {
            auto link_id = waypoint.GetLink(i);
            node.links[i] = link_id;

            if (link_id != 0)
                ++link_count;
        }

        if (link_count == 0)
            continue; // skip unlinked waypoints

        node.type = link_count == 2 ? mg::CHUNK_SPLINE_NODE_NORMAL : mg::CHUNK_SPLINE_NODE_JUNCTION;
        node.res_id = mg_res_->GetMeshes().GetIndexByName(link_count < 2 ? "deadend1" : "intersection1");

        params.nodes[id] = node;
    }
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

    auto& misc = root.children.emplace_back("misc");
    misc.children.emplace_back("lightpole", "lightpole");
    misc.children.emplace_back("woodfence", "woodfence");
    misc.children.emplace_back("woodfencepole", "woodfencepole");
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

void edit::Project::Deselect(Object& obj, bool fix_list)
{
    if (!obj.IsSelected())
        return;

    obj.SetSelected(false);

    if (fix_list)
        FixSelectionList();
}

void edit::Project::SelectOrDeselect(std::span<Object*> objs, bool additive, bool allow_deselect)
{
    if (!additive)
        ClearSelection();

    bool any_deselected = false;

    for (auto* obj : objs)
    {
        if (obj->IsSelected())
        {
            if (allow_deselect)
            {
                Deselect(*obj, false);
                any_deselected = true;
            }
        }
        else
        {
            Select(*obj);
        }
    }

    if (any_deselected)
        FixSelectionList();
}

void edit::Project::FixSelectionList()
{
    selection_.erase(std::remove_if(selection_.begin(), selection_.end(),
                                    [](const SelectionListEntry& entry) { return !entry.obj->IsSelected(); }),
                     selection_.end());
}

glm::mat4 edit::Project::GetNewObjectTransform(const glm::vec2& pos) const
{
    return glm::translate(glm::mat4(1.0f), glm::vec3(pos, GetTerrainHeight(pos)));
}

glm::mat4 edit::Project::SnapTransform(const glm::mat4& trans) const
{
    if (!properties_.snap_to_terrain_height)
        return trans;

    auto new_trans = trans;
    new_trans[3].z = GetTerrainHeight(glm::vec2(trans[3]));
    return new_trans;
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

    for (auto& [id, obj] : waypoints_)
    {
        all_objs_.emplace_back(&obj);
    }
}

static bool LineVsAABB(const glm::vec3& start, const glm::vec3& end, const AABB3& aabb)
{
    glm::vec3 dir = end - start;

    // Initialize bounds to the line segment [0.0 (start), 1.0 (end)]
    float tmin = 0.0f;
    float tmax = 1.0f;

    for (int i = 0; i < 3; ++i)
    {
        // If the line is parallel to the slab, handle carefully to avoid division by zero
        if (std::abs(dir[i]) < 1e-6f)
        {
            // If the segment's origin is completely outside the parallel slab, it's a miss
            if (start[i] < aabb.min[i] || start[i] > aabb.max[i])
            {
                return false;
            }
        }
        else
        {
            float inv_dir = 1.0f / dir[i];
            float t1 = (aabb.min[i] - start[i]) * inv_dir;
            float t2 = (aabb.max[i] - start[i]) * inv_dir;

            if (t1 > t2)
            {
                std::swap(t1, t2);
            }

            if (t1 > tmin)
                tmin = t1;
            if (t2 < tmax)
                tmax = t2;

            // If the valid interval becomes empty, there is no intersection
            if (tmin > tmax)
            {
                return false;
            }
        }
    }

    return true;
}

static float Distance2ToAABB(const glm::vec3& point, const AABB3& aabb)
{
    glm::vec3 closest = glm::clamp(point, aabb.min, aabb.max);
    glm::vec3 d = point - closest;
    return glm::dot(d, d);
}

edit::Object* edit::Project::ObjectRaycast(const glm::vec3& start, const glm::vec3& end)
{
    float nearest_dist2 = std::numeric_limits<float>().max();
    Object* nearest_obj = nullptr;

    for (auto& obj : all_objs_)
    {
        const auto& aabb = obj->GetAABB();

        if (!LineVsAABB(start, end, aabb))
            continue;

        auto dist2 = Distance2ToAABB(start, aabb);
        if (dist2 > nearest_dist2)
            continue;

        nearest_obj = obj;
        nearest_dist2 = dist2;
    }

    return nearest_obj;
}

void edit::Project::SetupChunks()
{
    map_config_.seed = 0;
    map_config_.chunks = 128;
    map_config_.chunk_size_m = 128.0f;
    map_config_.chunk_tiles = 128;

    height_sampler_.emplace(map_config_);
    chunk_manager_.emplace(mg_res_, map_config_, *this);
}

void edit::Project::UpdateChunks()
{
    constexpr std::array<float, 4> lod_distances = { 250.0f, 500.0f, 1000.0f, 2000.0f };

    chunk_manager_->BeginFrame(chunk_gen_time_);
    chunk_manager_->RequestArea(properties_.cam_pos_3d, lod_distances);
    chunk_manager_->EndFrame();

    chunk_gen_time_ += 1000; // TODO
}
