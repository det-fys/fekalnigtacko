#define GLM_ENABLE_EXPERIMENTAL

#include "map_project.hpp"

#include <algorithm>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include "assets/asset_manager.hpp"
#include "im/utils.hpp"
#include "map_viewport.hpp"
#include "object.hpp"
#include "static_object.hpp"

edit::Project::Project() : dynamics_world_(), map_(dynamics_world_, "openworld"), static_models_root_("root")
{
    while (!map_.IsLoaded())
    {
        map_.LoadNext();
    }

    InitStaticModels();
}

void edit::Project::Update()
{
    map_.Update();
}

void edit::Project::Draw(const gfx::DrawContext& ctx)
{
    for (auto& obj : objects_)
    {
        if (!ctx.frustum.IsSphereVisible({obj->GetPosition(), 5.0f}))
            continue;

        obj->Draw(ctx);
    }

    map_.SetDayTime(daytime_);
    world_env_.SetDayTime(daytime_);

    map_.Draw(ctx);
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
    return map_.GetChunkSize();
}

void edit::Project::DrawOverlay(const DrawOverlayContext& ctx)
{
    for (auto& obj : objects_)
    {
        if (!ctx.viewport.GetFrustum().IsSphereVisible({obj->GetPosition(), 5.0f}))
            continue;

        obj->DrawOverlay(ctx);
    }
}

void edit::Project::AddStaticObject(const glm::vec2& pos, const std::string& model_name)
{
    auto& obj = objects_.emplace_back(std::make_shared<StaticObject>(model_name));
    obj->SetTransform(glm::translate(glm::mat4(1.0f), glm::vec3(pos, 0.0f))); // TODO: Z from heightmap
    NewObjectAdded(*obj);
}

void edit::Project::MakeSelection2D(const glm::vec2& min, const glm::vec2* max)
{
    // find nearest obj
    float nearest_dist2 = std::numeric_limits<float>().max();
    Object* nearest_obj = nullptr;

    for (auto& obj : objects_)
    {
        auto d = glm::vec2(obj->GetPosition()) - min;
        auto dist2 = glm::dot(d, d);

        if (dist2 < nearest_dist2)
        {
            nearest_dist2 = dist2;
            nearest_obj = obj.get();
        }
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
    objects_.erase(std::remove_if(objects_.begin(), objects_.end(),
                                  [](const std::shared_ptr<Object>& obj) { return obj->IsSelected(); }),
                   objects_.end());

    selection_.clear();
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
    ClearSelection();
    Select(obj);
}
