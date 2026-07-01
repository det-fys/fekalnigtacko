#include "markerview.hpp"

#include "net/utils.hpp"
#include "assets/asset_manager.hpp"

game::view::MarkerView::MarkerView(WorldView& world, net::InMessage& msg) : Super(world, msg)
{
    if (!Init(msg))
    {
        throw EntityInitError();
    }
}

void game::view::MarkerView::Update(const UpdateInfo& info)
{
    float rotation_speed = marker_type_ == MARKER_PICKUP ? 2.0f : 0.5f;
    root_.local.rotation = glm::quat(glm::vec3(0.0f, 0.0f, info.time * rotation_speed));

    icon_node_.parent = &root_;

    if (marker_type_ == MARKER_PICKUP)
    {
        icon_node_.local.position.z = 0.6f + glm::sin(info.time * 2.0f) * 0.1f;
    }
    else
    {
        icon_node_.local.position.z = 1.0f;
    }

    root_.UpdateMatrix();
    icon_node_.UpdateMatrix();
}

void game::view::MarkerView::Draw(const DrawArgs& args)
{
    Super::Draw(args);

    if (base_model_)
    {
        DrawModel(args, *base_model_, root_);
    }
    
    if (model_)
    {
        DrawModel(args, *model_, icon_node_);
    }
}

bool game::view::MarkerView::Init(net::InMessage& msg)
{
    uint32_t color;
    net::ModelName model_name;
    if (!msg.Read(marker_type_) || !net::ReadPosition(msg, root_.local.position) || !net::ReadRGB(msg, color) || !msg.Read(model_name))
        return false;
    
    root_.UpdateMatrix();

    color_ = glm::unpackUnorm4x8(color | 0xFF000000);

    std::string model_name_str = model_name;
    if (!model_name_str.empty())
    {
        model_ = assets::AssetManager::GetInstance().Get<assets::Model>(model_name_str);
    }

    if (marker_type_ == MARKER_FOOT || marker_type_ == MARKER_VEHICLE)
    {
        base_model_ = assets::AssetManager::GetInstance().Get<assets::Model>("marker_base");
    }



    return true;
}

void game::view::MarkerView::DrawModel(const DrawArgs& args, const assets::Model& model, const TransformNode& node)
{
    const auto& mesh = *model.GetMesh();
    for (const auto& surface : mesh.surfaces)
    {
        gfx::DrawSurfaceCmd cmd;
        cmd.surface = &surface;
        cmd.matrices = &node.matrix;
        cmd.color = &color_;
        args.dlist.AddSurface(cmd);
    }
}
