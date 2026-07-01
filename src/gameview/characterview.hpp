#pragma once

#include "entityview.hpp"
#include "modelview.hpp"
#include "assets/item.hpp"
#include "assets/effect.hpp"
#include "game/skeletoninstance.hpp"
#include "skinning_ubo.hpp"
#include "game/character_anim_state.hpp"
#include "game/character_sync.hpp"

namespace game::view
{

struct CharacterViewState
{
    Transform trans;
    
    float loco_blend = 0.0f;
    float loco_phase = 0.0f;
    
    float action_time = 0.0f;

    float aim_yaw = 0.0f;
    float aim_pitch = 0.0f; 
};

struct CharacterViewClothes
{
    std::shared_ptr<const ModelView> model;
    glm::vec4 color = glm::vec4(1.0f);
    uint32_t surfacemask = 0;
};

class CharacterView : public EntityView
{
public:
    using Super = EntityView;
    using SurfaceMask = uint32_t;

    CharacterView(WorldView& world, net::InMessage& msg);
    DELETE_COPY_MOVE(CharacterView)

    virtual bool ProcessMsg(net::EntMsgType type, net::InMessage& msg) override;
    virtual bool ProcessUpdateMsg(net::InMessage* msg) override;
    virtual void Update(const UpdateInfo& info) override;
    virtual void Draw(const DrawArgs& args) override;

protected:
    virtual void OnAttach() override;

private:
    bool ReadState(net::InMessage* msg);

    SurfaceMask GetSurfaceMask(const std::string& name);
    void UpdateSurfaceMask();

    void AddClothes(const std::string& name, const glm::vec3& color);

    bool ProcessEquipMsg(net::InMessage& msg);
    bool ProcessFireMsg(net::InMessage& msg);

    void SetItem(const std::string& item_name);
    void DrawItem(const DrawArgs& args);
    void FireItem();

private:
    float yaw_ = 0.0f;

    std::shared_ptr<const ModelView> basemodel_;
    SkeletonInstance sk_;
    SkinningUBO ubo_;
    bool ubo_valid_ = false;

    CharacterAnimState animstate_;

    uint32_t surfacemask_ = 0xFFFFFFFF;
    std::vector<CharacterViewClothes> clothes_;

    // sync
    CharacterSyncState sync_;
    CharacterViewState states_[2];
    float update_time_ = 0.0f;

    std::string item_name_;
    TransformNode item_node_;
    TransformNode fire_snd_node_;
    std::shared_ptr<const assets::Item> item_;
    std::shared_ptr<const ModelView> item_model_;
    std::shared_ptr<const audio::Sound> fire_snd_;
    std::shared_ptr<const assets::Effect> fire_fx_;
    glm::vec3 fire_fx_offset_{};
};

}
