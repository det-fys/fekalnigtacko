#pragma once

#include "entity.hpp"
#include "marker_info.hpp"
#include "usable.hpp"
#include <memory>

namespace game
{

using MarkerQueryCallback = std::function<bool(PlayerCharacter&, UseTargetQueryResult&)>;
using MarkerUseCallback = std::function<void(PlayerCharacter&)>;

class Marker : public Entity, public Usable
{
public:
    using Super = Entity;

    Marker(World& world, const MarkerInfo& info);

    virtual void SendInitData(Player& player, net::OutMessage& msg) const override;
    virtual void Update() override;

    virtual bool QueryUseTarget(PlayerCharacter& character, uint32_t target_id, UseTargetQueryResult& res) override;
    virtual void Use(PlayerCharacter& character, uint32_t target_id) override;

    void SetUseTarget(const std::string& name, MarkerQueryCallback query, MarkerUseCallback use);
    void SetUseable(bool useable) { useable_ = useable; }

    virtual ~Marker() override;

private:
    MarkerInfo info_;

    std::unique_ptr<btCollisionObject> query_obj_;

    MarkerQueryCallback query_cb_;
    MarkerUseCallback use_cb_;

    bool useable_ = true;
};


}