#include "vehicleview.hpp"

#include "assets/cache.hpp"

#include <iostream>

game::view::VehicleView::VehicleView(WorldView& world, std::shared_ptr<const assets::VehicleModel> model)
    : EntityView(world), model_(std::move(model))
{
}

std::unique_ptr<game::view::VehicleView> game::view::VehicleView::InitFromMsg(WorldView& world, net::InMessage& msg)
{
    net::ModelName modelname;
    if (!msg.Read(modelname))
        return nullptr;

    auto model = assets::CacheManager::GetVehicleModel("data/" + std::string(modelname) + ".veh");

    return std::make_unique<VehicleView>(world, std::move(model));
}

bool game::view::VehicleView::ProcessMsg(net::EntMsgType type, net::InMessage& msg)
{
    switch (type)
    {
    case net::EMSG_UPDATE:
        return ProcessUpdateMsg(msg);

    default:
        return false;
    }
}

void game::view::VehicleView::Update() {}

void game::view::VehicleView::Draw(gfx::DrawList& dlist)
{
    root_.UpdateMatrix();

    // TOOD: chceck and fix
    const auto& model = *model_->GetModel();
    const auto& mesh = *model.GetMesh();

    for (const auto& surface : mesh.surfaces)
    {
        gfx::DrawSurfaceCmd cmd;
        cmd.surface = &surface;
        cmd.matrices = &root_.matrix;
        dlist.AddSurface(cmd);
    }

    const auto& wheels = model_->GetWheels();
    for (size_t i = 0; i < 4; ++i)
    {
        wheels_[i].UpdateMatrix();

        const auto& mesh = *wheels[i].model->GetMesh();

        for (const auto& surface : mesh.surfaces)
        {
            gfx::DrawSurfaceCmd cmd;
            cmd.surface = &surface;
            cmd.matrices = &wheels_[i].matrix;
            dlist.AddSurface(cmd);
        }
    }
}

bool game::view::VehicleView::ProcessUpdateMsg(net::InMessage& msg)
{
    auto& trans = root_.local;
    glm::vec3 angles;

    if (!msg.Read<net::PositionQ>(trans.position.x) || !msg.Read<net::PositionQ>(trans.position.y) ||
        !msg.Read<net::PositionQ>(trans.position.z) || !msg.Read<net::AngleQ>(angles.x) ||
        !msg.Read<net::AngleQ>(angles.y) || !msg.Read<net::AngleQ>(angles.z))
        return false;

    trans.rotation = glm::quat(angles);

    for (size_t i = 0; i < 4; ++i)
    {
        auto& trans = wheels_[i].local;
        glm::vec3 angles;

        if (!msg.Read<net::PositionQ>(trans.position.x) || !msg.Read<net::PositionQ>(trans.position.y) ||
            !msg.Read<net::PositionQ>(trans.position.z) || !msg.Read<net::AngleQ>(angles.x) ||
            !msg.Read<net::AngleQ>(angles.y) || !msg.Read<net::AngleQ>(angles.z))
            return false;

        trans.rotation = glm::quat(angles);
    }

}
