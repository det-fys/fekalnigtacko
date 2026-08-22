#pragma once

#include <vector>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtx/hash.hpp>

#include "collision/dynamicsworld.hpp"
#include "gameview/mapinstanceview.hpp"
#include "gameview/modelview.hpp"
#include "gameview/worldenv.hpp"
#include "gfx/scene.hpp"
#include "mapgen/chunk.hpp"
#include "static_object.hpp"


namespace edit
{

using namespace game::view;

struct Chunk
{
    mg::Chunk mgchunk;

    std::shared_ptr<const ModelView> model;

    // visualization
    std::vector<glm::vec2> vis_verts;
    std::vector<std::tuple<uint32_t, uint32_t>> vis_edges;
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> vis_tris;
};

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
    Project();

    void Update();

    // Scene
    virtual void Draw(const gfx::DrawContext& ctx) override;
    virtual gfx::Environment GetSceneEnvironment() override;
    virtual float GetMapChunkSize() override;

    void DrawOverlay(const DrawOverlayContext& ctx);

    // world
    void SetDayTime(float daytime) { daytime_ = daytime; }
    void SetDrawWorldEnv(bool draw) { draw_world_env_ = draw; }

    // assets
    const StaticModelsEntry& GetStaticModelsRoot() const { return static_models_root_; }

    // new objects
    void AddStaticObject(const glm::vec2& pos, const std::string& model_name);

    // selection operations
    void MakeSelection2D(const glm::vec2& min, const glm::vec2* max);
    bool HasSelection() const;
    const glm::mat4* GetSelectionMatrix() const;
    void ApplySelectionTransform(const glm::mat4& delta);
    void ClearSelection();
    void DeleteSelection();

    // generation
    void InvalidateChunk(const glm::ivec2& chunk_pos);
    void DelayChunkUpdates();

    const std::unordered_map<glm::ivec2, Chunk>& GetChunks() const { return chunks_; }
    const mg::MapConfig& GetMapConfig() const { return map_config_; }

private:
    void InitStaticModels();

    void Select(Object& obj);
    void Deselect(Object& obj);
    void FixSelectionList();

    void NewObjectAdded(Object& obj);
    void UpdateAllObjectsList();

    // generation
    void SetupChunks(uint32_t size);
    void UpdateChunks();
    void UpdateChunk(const glm::ivec2& chunk_pos);

private:
    StaticModelsEntry static_models_root_;

    WorldEnv world_env_;
    float daytime_ = 12.0f; // 0-24
    bool draw_world_env_ = true;

    // object lists
    std::vector<StaticObject> static_objs_;
    std::vector<Object*> all_objs_; // pointers to all objects 4 ez iteration

    // selection & manipulation
    std::vector<SelectionListEntry> selection_;
    ImGuizmo::OPERATION gizmo_operation_ = ImGuizmo::TRANSLATE;


    // chunk generation
    mg::MapConfig map_config_{};
    std::unordered_map<glm::ivec2, Chunk> chunks_;
    std::unordered_set<glm::ivec2> invalid_chunks_;
    float chunk_update_time_ = 0.0f;
};

} // namespace edit
