#include "characterview.hpp"
#include "assets/cache.hpp"
#include "net/utils.hpp"

game::view::CharacterView::CharacterView(WorldView& world, net::InMessage& msg) : EntityView(world, msg)
{

    sk_ = SkeletonInstance(assets::CacheManager::GetSkeleton("data/human.sk"), &root_);
}

bool game::view::CharacterView::ProcessMsg(net::EntMsgType type, net::InMessage& msg)
{
    switch (type)
    {
    case net::EMSG_UPDATE:
        return ProcessUpdateMsg(msg);

    default:
        return Super::ProcessMsg(type, msg);
    }
}

void game::view::CharacterView::Update(const UpdateInfo& info)
{
    auto anim = sk_.GetSkeleton()->GetAnimation("walk");
    sk_.ApplySkelAnim(*anim, info.time, 1.0f);
    root_.UpdateMatrix();
    sk_.UpdateBoneMatrices();
}

void game::view::CharacterView::Draw(const DrawArgs& args)
{
    Super::Draw(args);

    glm::vec3 start = root_.local.position;
    glm::vec3 end = start + glm::vec3(0.0f, 0.0f, 1.5f);
    args.dlist.AddBeam(start, end, 0xFF777777, 0.1f);

    start = root_.local.position;
    end = start + glm::vec3(glm::cos(yaw_), glm::sin(yaw_), 0.0f) * 0.5f;
    args.dlist.AddBeam(start, end, 0xFF007700, 0.05f);

    // draw bones debug
    const auto& bone_nodes = sk_.GetBoneNodes();
    for (const auto& bone_node : bone_nodes)
    {
        if (!bone_node.parent)
            continue;

        glm::vec3 p0 = bone_node.parent->matrix[3];
        glm::vec3 p1 = bone_node.matrix[3];

        args.dlist.AddBeam(p0, p1, 0xFF00EEEE, 0.01f);
    }
}

bool game::view::CharacterView::ProcessUpdateMsg(net::InMessage& msg)
{
    net::PositionQ posq;
    if (!net::ReadPositionQ(msg, posq) || !msg.Read<net::PositiveAngleQ>(yaw_))
        return false;

    net::DecodePosition(posq, root_.local.position);

    return true;
}
