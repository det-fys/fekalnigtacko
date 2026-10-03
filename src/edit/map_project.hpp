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
    glm::mat4 offset{1.0f};

    SelectionListEntry(Object* obj, const glm::mat4& offset) : obj(obj), offset(offset) {}
};

struct StaticModelsEntry
{
    std::string display_name;
    std::string model_name;
    std::vector<StaticModelsEntry> children;

    StaticModelsEntry(std::string display_name) : display_name(std::move(display_name)) {}

    StaticModelsEntry(std::string display_name, std::string model_name)
        : display_name(std::move(display_name)), model_name(std::move(model_name))
    {
    }
};

class Project : public gfx::Scene
{
public:
    Project(MapEditProperties& properties);

    void Update();

    // Scene
    virtual void Draw(const gfx::DrawContext& ctx) override;
    virtual gfx::Environment GetSceneEnvironment() override;
    virtual float GetMapChunkSize() override;

    void DrawOverlay(const DrawOverlayContext& ctx);

    // world
    float GetTerrainHeight(const glm::vec2& pos) const;

    // assets
    const StaticModelsEntry& GetStaticModelsRoot() const { return static_models_root_; }

    // objects
    void AddStaticObject(const glm::mat4& trans, const std::string& model_name);
    void AddStaticObject(const glm::vec2& pos, const std::string& model_name);
    Waypoint* AddWaypoint(const glm::mat4& trans);
    Waypoint* AddWaypoint(const glm::vec2& pos);

    Waypoint* GetWaypoint(WaypointID id);

    // selection operations
    void SetHover(const glm::vec3& start, const glm::vec3& end);
    void MakeSelection(const glm::vec3& start, const glm::vec3& end, bool additive);
    void MakeSelection2D(const glm::vec2& min, const glm::vec2* max, bool additive);
    bool HasSelection() const;
    const glm::mat4* GetSelectionMatrix() const;
    void ApplySelectionTransform(const glm::mat4& delta);
    void CloneSelection(const glm::mat4& trans);
    void CloneSelection(const glm::vec2& pos);
    void LinkSelection(bool link);
    void ClearSelection();
    void DeleteSelection();

    // generation
    void InvalidateChunk(const glm::ivec2& chunk_pos);
    void GetChunkParams(mg::ChunkParams& out_params);

    const std::unordered_map<glm::ivec2, ProjectChunkData>& GetChunks() const { return chunk_manager_->GetChunks(); }
    mg::ChunkState GetChunkState(const glm::ivec2& chunk_pos) const { return chunk_manager_->GetChunkState(chunk_pos); }
    const mg::MapConfig& GetMapConfig() const { return map_config_; }

private:
    void InitStaticModels();

    void Select(Object& obj);
    void Deselect(Object& obj, bool fix_list = true);
    void SelectOrDeselect(std::span<Object*> objs, bool additive, bool allow_deselect);
    void FixSelectionList();

    glm::mat4 GetNewObjectTransform(const glm::vec2& pos) const;
    glm::mat4 SnapTransform(const glm::mat4& trans) const;
    void NewObjectAdded(Object& obj);
    void UpdateAllObjectsList();

    Object* ObjectRaycast(const glm::vec3& start, const glm::vec3& end);

    // generation
    void SetupChunks();
    void UpdateChunks();

private:
    MapEditProperties& properties_;

    std::shared_ptr<const mg::ResourceSet> mg_res_;
    StaticModelsEntry static_models_root_;

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
