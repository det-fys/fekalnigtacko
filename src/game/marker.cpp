#include "marker.hpp"

#include "world.hpp"
#include "net/utils.hpp"

static const glm::mat4 identity(1.0f);

game::Marker::Marker(World& world, const MarkerInfo& info) : Super(world, net::ET_MARKER), Usable(identity), info_(info) 
{
    root_.local.position = info_.position;
    root_.UpdateMatrix();

    max_distance_ = 150.0f;
}

void game::Marker::SendInitData(Player& player, net::OutMessage& msg) const
{
    Super::SendInitData(player, msg);

    msg.Write(info_.type);
    net::WritePosition(msg, info_.position);
    net::WriteRGB(msg, info_.color);
    msg.Write(net::ModelName(info_.model));
}

void game::Marker::Update()
{
    Super::Update();

}

bool game::Marker::QueryUseTarget(PlayerCharacter& character, uint32_t target_id, UseTargetQueryResult& res)
{
    if (!useable_)
        return false;

    if (!query_cb_)
        return false;

    return query_cb_(character, res);
}

void game::Marker::Use(PlayerCharacter& character, uint32_t target_id)
{
    if (!use_cb_)
        return;

    use_cb_(character);
}

void game::Marker::SetUseTarget(const std::string& name, MarkerQueryCallback query, MarkerUseCallback use)
{
    if (!query_obj_)
    {
        query_obj_ = std::make_unique<btCollisionObject>();

        static btSphereShape query_sphere(0.01f);
        query_obj_->setCollisionShape(&query_sphere);
        query_obj_->setWorldTransform(root_.local.ToBtTransform());
        query_obj_->setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);
        collision::SetObjectInfo(query_obj_.get(), collision::OT_ENTITY, collision::OF_USABLE, this);

        world_.GetBtWorld().addCollisionObject(query_obj_.get());
    }

    query_cb_ = query;
    use_cb_ = use;

    use_targets_.clear();
    use_targets_.emplace_back(this, 0, root_.GetGlobalPosition(), name);
}

game::Marker::~Marker()
{
    if (query_obj_)
    {
        world_.GetBtWorld().removeCollisionObject(query_obj_.get());
    }

}
