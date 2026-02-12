#pragma once

#include "entityview.hpp"
#include "assets/model.hpp"
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
};

struct CharacterViewClothes
{
    std::shared_ptr<const assets::Model> model;
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
    virtual void Update(const UpdateInfo& info) override;
    virtual void Draw(const DrawArgs& args) override;

private:
    bool ReadState(net::InMessage& msg);
    bool ProcessUpdateMsg(net::InMessage& msg);

    SurfaceMask GetSurfaceMask(const std::string& name);
    void UpdateSurfaceMask();

    void AddClothes(const std::string& name, const glm::vec3& color);

private:
    float yaw_ = 0.0f;

    std::shared_ptr<const assets::Model> basemodel_;
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
};

}
