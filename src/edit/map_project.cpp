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
#include "mapgen/map_layout_file.hpp"

edit::Project::Project(MapEditProperties& properties) : properties_(properties)
{
    mg_res_ = assets::AssetManager::GetInstance().Get<mg::ResourceSet>("resources");
    SetupChunks();
}

edit::Project::Project(MapEditProperties& properties, const std::string& name, const std::string& file_name)
    : Project(properties)
{
    SetName(name, file_name);
    Load(file_name_);
}

bool edit::Project::Save()
{
    if (file_name_.empty())
        return false;

    Save(file_name_);
    return true;
}

void edit::Project::SetName(const std::string& name, const std::string& file_name)
{
    name_ = name;
    file_name_ = file_name;
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

void edit::Project::ShowSelectionProperties()
{
    if (selection_.empty())
    {
        ImGui::Text("nothing selected");
        return;
    }

    auto& selected_obj = *selection_.front().obj;
    selected_obj.ShowProperties();
}

float edit::Project::GetTerrainHeight(const glm::dvec2& pos) const
{
    if (!height_sampler_)
        return 0.0f;

    return height_sampler_->Get(pos);
}

void edit::Project::AddStaticObject(const FarMatrix& trans, uint32_t model_id)
{
    ClearSelection(); // to avoid invalid pointers
    auto& obj = static_objs_.emplace_back(*this, trans, model_id);
    NewObjectAdded(obj);
}

void edit::Project::AddStaticObject(const glm::dvec2& pos, uint32_t model_id)
{
    AddStaticObject(GetNewObjectTransform(pos), model_id);
}

edit::Waypoint* edit::Project::AddWaypoint(const FarMatrix& trans)
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

edit::Waypoint* edit::Project::AddWaypoint(const glm::dvec2& pos)
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

void edit::Project::SetHover(const glm::dvec3& start, const glm::dvec3& end)
{
    Object* hovered_obj = ObjectRaycast(start, end);

    for (auto& obj : all_objs_)
    {
        obj->SetHovered(obj == hovered_obj);
    }
}

void edit::Project::MakeSelection(const glm::dvec3& start, const glm::dvec3& end, bool additive)
{
    auto obj = ObjectRaycast(start, end);
    SelectOrDeselect({&obj, static_cast<size_t>(obj ? 1 : 0)}, additive, true);
}

void edit::Project::MakeSelection2D(const glm::dvec2& min, const glm::dvec2* max, bool additive)
{
    MakeSelection(glm::dvec3(min, 1000.0), glm::dvec3(min, 1000.0), additive);
}

bool edit::Project::HasSelection() const
{
    return !selection_.empty();
}

const FarMatrix* edit::Project::GetSelectionMatrix() const
{
    if (!HasSelection())
        return nullptr;

    return &selection_.front().obj->GetTransform();
}

void edit::Project::ApplySelectionTransform(const FarMatrix& delta)
{
    if (!HasSelection())
        return;

    auto& first_entry = selection_.front();
    auto& first_obj = *first_entry.obj;

    auto first_trans = delta * first_obj.GetTransform();
    first_trans.NormalizeBasis();

    first_obj.SetTransform(SnapTransform(first_trans));

    for (uint32_t i = 1; i < selection_.size(); ++i)
    {
        auto& entry = selection_[i];
        auto& obj = entry.obj;
        obj->SetTransform(SnapTransform(first_trans * entry.offset));
    }
}

void edit::Project::CloneSelection(const FarMatrix& trans)
{
    if (!HasSelection())
        return;

    // currently can only clone a single obj due to selection invalidation issues
    selection_[0].obj->Clone(trans);
}

void edit::Project::CloneSelection(const glm::dvec2& pos)
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

mg::MapLayout edit::Project::GetMapLayout(const AABBD2* aabb) const
{
    mg::MapLayout map{};
    map.cfg = map_config_;

    // static objs
    for (const auto& obj : static_objs_)
    {
        if (aabb)
        {
            const auto& obj_aabb = obj.GetAABB();
            AABBD2 obj_aabb_2d(glm::dvec2(obj_aabb.min), glm::dvec2(obj_aabb.max));

            if (!aabb->CollidesWith(obj_aabb_2d))
                continue;
        }

        auto& mgobj = map.objs.emplace_back();
        mgobj.trans = obj.GetTransform();
        mgobj.model_id = obj.GetModelID();
    }

    // waypoints
    for (const auto& [id, waypoint] : waypoints_)
    {
        auto& node = map.nodes[id];
        node.pos = waypoint.GetPosition();
        node.mesh_id = waypoint.GetMeshID();

        for (uint32_t i = 0; i < 4; ++i)
        {
            node.links[i] = waypoint.GetLink(i);
        }
    }

    return map;
}

void edit::Project::GetChunkParams(mg::ChunkParams& params)
{
    AABBD2 chunk_aabb = mg::GetChunkAABB(map_config_, params.coord, true);
    params.map = GetMapLayout(&chunk_aabb);
}

void edit::Project::Load(const std::string& file_name)
{
    SetMapLayout(mg::LoadMapLayout(file_name, *mg_res_));
}

void edit::Project::Save(const std::string& file_name)
{
    mg::SaveMapLayout(file_name, *mg_res_, GetMapLayout());
}

void edit::Project::SetMapLayout(const mg::MapLayout& map)
{
    static_objs_.clear();
    waypoints_.clear();

    // OBJS
    for (const auto& obj : map.objs)
    {
        static_objs_.emplace_back(*this, obj.trans, obj.model_id);
    }

    // NODES
    for (const auto& [id, node] : map.nodes)
    {
        auto& wp = waypoints_.emplace(id, Waypoint(*this, node.pos, id)).first->second;

        // links
        for (uint32_t i = 0; i < 4; ++i)
        {
            wp.SetLink(i, node.links[i]);
        }

        // mesh id
        wp.SetMeshID(node.mesh_id);
    }

    UpdateAllObjectsList();
}

void edit::Project::Select(Object& obj)
{
    if (obj.IsSelected())
        return;

    obj.SetSelected(true);

    auto selection_matrix = GetSelectionMatrix();
    auto offset = selection_matrix ? selection_matrix->Invert() * obj.GetTransform() : FarMatrix(1.0);

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

FarMatrix edit::Project::GetNewObjectTransform(const glm::dvec2& pos) const
{
    return FarMatrix(glm::dvec3(pos, GetTerrainHeight(pos)));
}

FarMatrix edit::Project::SnapTransform(const FarMatrix& trans) const
{
    if (!properties_.snap_to_terrain_height)
        return trans;

    auto new_trans = trans;
    new_trans.position.z = GetTerrainHeight(trans.position);
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

static bool LineVsAABB(const glm::dvec3& start, const glm::dvec3& end, const AABB3& aabb)
{
    glm::dvec3 dir = end - start;

    auto aabb_min = glm::dvec3(aabb.min);
    auto aabb_max = glm::dvec3(aabb.max);

    // Initialize bounds to the line segment [0.0 (start), 1.0 (end)]
    double tmin = 0.0;
    double tmax = 1.0;

    for (int i = 0; i < 3; ++i)
    {
        // If the line is parallel to the slab, handle carefully to avoid division by zero
        if (std::abs(dir[i]) < 1e-6)
        {
            // If the segment's origin is completely outside the parallel slab, it's a miss
            if (start[i] < aabb_min[i] || start[i] > aabb_max[i])
            {
                return false;
            }
        }
        else
        {
            double inv_dir = 1.0 / dir[i];
            double t1 = (aabb_min[i] - start[i]) * inv_dir;
            double t2 = (aabb_max[i] - start[i]) * inv_dir;

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

static double Distance2ToAABB(const glm::dvec3& point, const AABB3& aabb)
{
    glm::dvec3 closest = glm::clamp(point, glm::dvec3(aabb.min), glm::dvec3(aabb.max));
    glm::dvec3 d = point - closest;
    return glm::dot(d, d);
}

edit::Object* edit::Project::ObjectRaycast(const glm::dvec3& start, const glm::dvec3& end)
{
    auto nearest_dist2 = std::numeric_limits<double>().max();
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
    constexpr std::array<double, 4> lod_distances = { 250.0, 500.0, 1000.0, 2000.0 };

    chunk_manager_->BeginFrame(chunk_gen_time_);
    chunk_manager_->RequestArea(properties_.cam_pos_3d, lod_distances);
    chunk_manager_->EndFrame();

    chunk_gen_time_ += 1000; // TODO
}
