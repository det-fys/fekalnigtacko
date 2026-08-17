#pragma once

#include <vector>
#include <memory>

#include <imgui.h>
#include <ImGuizmo.h>

#include "collision/dynamicsworld.hpp"
#include "gameview/mapinstanceview.hpp"
#include "gameview/modelview.hpp"
#include "gameview/worldenv.hpp"
#include "gfx/scene.hpp"

namespace edit
{

using namespace game::view;

class Object;
struct DrawOverlayContext;

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

private:
    void InitStaticModels();

    void Select(Object& obj);
    void Deselect(Object& obj);
    void FixSelectionList();

    void NewObjectAdded(Object& obj);

private:
    collision::DynamicsWorld dynamics_world_;
    MapInstanceView map_;
    WorldEnv world_env_;

    StaticModelsEntry static_models_root_;

    float daytime_ = 12.0f; // 0-24

    std::vector<std::shared_ptr<Object>> objects_;
    std::vector<SelectionListEntry> selection_;

    ImGuizmo::OPERATION gizmo_operation_ = ImGuizmo::TRANSLATE;
};

} // namespace edit
