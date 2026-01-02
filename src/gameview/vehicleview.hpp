#pragma once

#include "entityview.hpp"

#include "assets/vehiclemdl.hpp"

#include <chrono>

namespace game::view
{

class VehicleView : public EntityView
{
public:
    VehicleView(WorldView& world, std::shared_ptr<const assets::VehicleModel> model);
    static std::unique_ptr<VehicleView> InitFromMsg(WorldView& world, net::InMessage& msg);

    virtual bool ProcessMsg(net::EntMsgType type, net::InMessage& msg) override;
    virtual void Update() override;
    virtual void Draw(gfx::DrawList& dlist) override;

private:
    bool ProcessUpdateMsg(net::InMessage& msg);

private:
    std::shared_ptr<const assets::VehicleModel> model_;

    TransformNode wheels_[4];

};

}