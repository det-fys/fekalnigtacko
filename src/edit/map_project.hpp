#pragma once

#include <vector>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtx/hash.hpp>

#include "map_edit_properties.hpp"
#include "collision/dynamicsworld.hpp"
#include "gameview/mapinstanceview.hpp"
#include "gameview/modelview.hpp"
#include "gameview/worldenv.hpp"
#include "gfx/scene.hpp"
#include "mapgen/resources.hpp"
#include "static_object.hpp"
#include "waypoint.hpp"
#include "utils/allocnum.hpp"
#include "mapgen/samplers.hpp"
#include "chunk_manager_project.hpp"

namespace edit
{

using namespace game::view;

struct SelectionListEntry
{
    Object* obj;
    FarMatrix offset{1.0};

    SelectionListEntry(Object* obj, const FarMatrix& offset) : obj(obj), offset(offset) {}
};

class Project : public gfx::Scene
{
public:
    Project(MapEditProperties& properties); // new
    Project(MapEditProperties& properties, const std::string& name, const std::string& file_name); // load

    bool Save();
    void SetName(const std::string& name, const std::string& file_name);

    void Update();

    // Scene
    virtual void Draw(const gfx::DrawContext& ctx) override;
    virtual gfx::Environment GetSceneEnvironment() override;
    virtual float GetMapChunkSize() override;

    void DrawOverlay(const DrawOverlayContext& ctx);

    void ShowSelectionProperties();

    // world
    float GetTerrainHeight(const glm::dvec2& pos) const;

    // objects
    void AddStaticObject(const FarMatrix& trans, uint32_t model_id);
    void AddStaticObject(const glm::dvec2& pos, uint32_t model_id);
    Waypoint* AddWaypoint(const FarMatrix& trans);
    Waypoint* AddWaypoint(const glm::dvec2& pos);

    Waypoint* GetWaypoint(WaypointID id);

    // selection operations
    void SetHover(const glm::dvec3& start, const glm::dvec3& end);
    void MakeSelection(const glm::dvec3& start, const glm::dvec3& end, bool additive);
    void MakeSelection2D(const glm::dvec2& min, const glm::dvec2* max, bool additive);
    bool HasSelection() const;
    const FarMatrix* GetSelectionMatrix() const;
    void ApplySelectionTransform(const FarMatrix& delta);
    void CloneSelection(const FarMatrix& trans);
    void CloneSelection(const glm::dvec2& pos);
    void LinkSelection(bool link);
    void ClearSelection();
    void DeleteSelection();

    // generation
    void InvalidateChunk(const glm::ivec2& chunk_pos);
    mg::MapLayout GetMapLayout(const AABBD2* aabb = nullptr) const;
    void GetChunkParams(mg::ChunkParams& out_params);

    const std::unordered_map<glm::ivec2, ProjectChunkData>& GetChunks() const { return chunk_manager_->GetChunks(); }
    mg::ChunkState GetChunkState(const glm::ivec2& chunk_pos) const { return chunk_manager_->GetChunkState(chunk_pos); }
    const mg::MapConfig& GetMapConfig() const { return map_config_; }
    const mg::ResourceSet& GetResourceSet() const { return *mg_res_; }

private:
    void Load(const std::string& file_name);
    void Save(const std::string& file_name);

    void SetMapLayout(const mg::MapLayout& map);
    
    void Select(Object& obj);
    void Deselect(Object& obj, bool fix_list = true);
    void SelectOrDeselect(std::span<Object*> objs, bool additive, bool allow_deselect);
    void FixSelectionList();

    FarMatrix GetNewObjectTransform(const glm::dvec2& pos) const;
    FarMatrix SnapTransform(const FarMatrix& trans) const;
    void NewObjectAdded(Object& obj);
    void UpdateAllObjectsList();

    Object* ObjectRaycast(const glm::dvec3& start, const glm::dvec3& end);

    // generation
    void SetupChunks();
    void UpdateChunks();

private:
    MapEditProperties& properties_;

    std::shared_ptr<const mg::ResourceSet> mg_res_;

    std::string name_;
    std::string file_name_;

    std::optional<mg::TerrainHeightSampler> height_sampler_;

    WorldEnv world_env_;

    // object lists
    std::vector<StaticObject> static_objs_;
    std::map<WaypointID, Waypoint> waypoints_;
    WaypointID last_waypoint_id_ = 0;
    std::vector<Object*> all_objs_; // pointers to all objects 4 ez iteration

    // selection & manipulation
    std::vector<SelectionListEntry> selection_;
    ImGuizmo::OPERATION gizmo_operation_ = ImGuizmo::TRANSLATE;

    // chunk generation
    mg::MapConfig map_config_{};
    int64_t chunk_gen_time_ = 0;
    std::optional<ProjectChunkManager> chunk_manager_;
    

};

} // namespace edit
