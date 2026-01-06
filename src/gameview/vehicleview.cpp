#include "vehicleview.hpp"

#include "assets/cache.hpp"
#include "net/utils.hpp"
#include "worldview.hpp"

#include <iostream>

game::view::VehicleView::VehicleView(WorldView& world, std::shared_ptr<const assets::VehicleModel> model)
    : EntityView(world), model_(std::move(model))
{
    auto& modelwheels = model_->GetWheels();
    wheels_.resize(modelwheels.size());

    for (size_t i = 0; i < wheels_.size(); ++i)
    {
        wheels_[i].node.parent = &root_;
    }
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

void game::view::VehicleView::Update(const UpdateInfo& info)
{
    float tps = 25.0f;
    float t = (info.time - update_time_) * tps * 0.8f; // assume some jitter, interpolate for longer
    t = glm::clamp(t, 0.0f, 2.0f);
    root_.local = Transform::Lerp(root_trans_[0], root_trans_[1], t);

    root_.UpdateMatrix();

    const auto& wheels = model_->GetWheels();
    for (size_t i = 0; i < wheels.size(); ++i)
    {
        // update wheel transform
        auto& wheelstate = wheels_[i];
        auto& wheeltrans = wheelstate.node.local;
        wheeltrans.position = wheels[i].position;
        wheeltrans.position.z += wheelstate.z_offset;
        
        // rotate
        wheelstate.rotation += info.delta_time * wheelstate.speed;

        wheeltrans.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        wheeltrans.rotation = glm::rotate(wheeltrans.rotation, wheelstate.steering, glm::vec3(0, 0, 1));
        wheeltrans.rotation = glm::rotate(wheeltrans.rotation, wheelstate.rotation, glm::vec3(1, 0, 0));

        wheels_[i].node.UpdateMatrix();
    }
}

void game::view::VehicleView::Draw(gfx::DrawList& dlist)
{
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
    for (size_t i = 0; i < wheels.size(); ++i)
    {
        const auto& mesh = *wheels[i].model->GetMesh();

        for (const auto& surface : mesh.surfaces)
        {
            gfx::DrawSurfaceCmd cmd;
            cmd.surface = &surface;
            cmd.matrices = &wheels_[i].node.matrix;
            dlist.AddSurface(cmd);
        }
    }
}

bool game::view::VehicleView::ProcessUpdateMsg(net::InMessage& msg)
{
    root_trans_[0] = root_.local;
    auto& root_trans = root_trans_[1];
    update_time_ = world_.GetTime();

    if (!net::ReadTransform(msg, root_trans))
        return false;

    float steering;
    if (!msg.Read<net::AngleQ>(steering))
        return false;

    const auto& wheels = model_->GetWheels();
    for (size_t i = 0; i < wheels_.size(); ++i)
    {
        auto& wheel = wheels_[i];
        if (!msg.Read<net::WheelZOffsetQ>(wheel.z_offset) || !msg.Read<net::RotationSpeedQ>(wheel.speed))
            return false;

        wheel.steering = i < 2 ? steering : 0.0f;
    }

}
