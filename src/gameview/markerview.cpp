#include "markerview.hpp"

#include "net/utils.hpp"
#include "assets/cache.hpp"

game::view::MarkerView::MarkerView(WorldView& world, net::InMessage& msg) : Super(world, msg)
{
    if (!Init(msg))
    {
        throw EntityInitError();
    }
}

void game::view::MarkerView::Update(const UpdateInfo& info)
{
    root_.local.rotation = glm::quat(glm::vec3(0.0f, 0.0f, info.time * 0.5f));
    root_.UpdateMatrix();
}

void game::view::MarkerView::Draw(const DrawArgs& args)
{
    Super::Draw(args);

    if (!model_)
        return;

    const auto& mesh = *model_->GetMesh();
    for (const auto& surface : mesh.surfaces)
    {
        gfx::DrawSurfaceCmd cmd;
        cmd.surface = &surface;
        cmd.matrices = &root_.matrix;
        cmd.color = &color_;
        args.dlist.AddSurface(cmd);
    }

}

bool game::view::MarkerView::Init(net::InMessage& msg)
{
    net::PositionQ pos_q;
    uint32_t color;

    if (!msg.Read(marker_type_) || !net::ReadPositionQ(msg, pos_q) || !net::ReadRGB(msg, color))
        return false;
    
    net::DecodePosition(pos_q, root_.local.position);
    root_.UpdateMatrix();

    color_ = glm::unpackUnorm4x8(color | 0xFF000000);

    if (marker_type_ == MARKER_FOOT || marker_type_ == MARKER_VEHICLE)
    {
        model_ = assets::CacheManager::GetModel("data/marker_tuning.mdl");
    }

    return true;
}
