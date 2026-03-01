#include "simple_entity.hpp"
#include "net/utils.hpp"

game::SimpleEntity::SimpleEntity(World& world, const std::string& modelname) : Super(world, net::ET_SIMPLE), modelname_(modelname)
{
    UpdateSyncState();
}

void game::SimpleEntity::SendInitData(Player& player, net::OutMessage& msg) const
{
    Super::SendInitData(player, msg);

    msg.Write(net::ModelName(modelname_));

    // write state against default
    static const SimpleEntitySyncState default_state;
    size_t fields_pos = msg.Reserve<SimpleEntitySyncFieldFlags>();
    auto fields = WriteState(msg, default_state);
    msg.WriteAt(fields_pos, fields);
}

void game::SimpleEntity::Update()
{
    Super::Update();

    UpdatePreSync(); // chance to update state before sent

    sync_current_ = 1 - sync_current_;
    UpdateSyncState();
    SendUpdateMsg();

}

void game::SimpleEntity::UpdateSyncState()
{
    SimpleEntitySyncState& state = sync_[sync_current_];
        
    net::EncodePosition(root_.local.position, state.pos);
    net::EncodeRotation(root_.local.rotation, state.rot);
}

game::SimpleEntitySyncFieldFlags game::SimpleEntity::WriteState(net::OutMessage& msg, const SimpleEntitySyncState& base) const
{
    SimpleEntitySyncFieldFlags fields = 0;
    const SimpleEntitySyncState& curr = sync_[sync_current_];

    if (curr.pos.x.value != base.pos.x.value ||
        curr.pos.y.value != base.pos.y.value ||
        curr.pos.z.value != base.pos.z.value)
    {
        fields |= SESF_POSITION;

        net::WriteDelta(msg, curr.pos.x, base.pos.x);
        net::WriteDelta(msg, curr.pos.y, base.pos.y);
        net::WriteDelta(msg, curr.pos.z, base.pos.z);
    }

    if (curr.rot.x.value != base.rot.x.value ||
        curr.rot.y.value != base.rot.y.value ||
        curr.rot.z.value != base.rot.z.value)
    {
        fields |= SESF_ROTATION;

        net::WriteDelta(msg, curr.rot.x, base.rot.x);
        net::WriteDelta(msg, curr.rot.y, base.rot.y);
        net::WriteDelta(msg, curr.rot.z, base.rot.z);
    }

    return fields;
}

void game::SimpleEntity::SendUpdateMsg()
{
    auto msg = BeginUpdateMsg();

    // write state against previous
    const SimpleEntitySyncState& prev = sync_[1 - sync_current_];
    size_t fields_pos = msg.Reserve<SimpleEntitySyncFieldFlags>();
    auto fields = WriteState(msg, prev);
    
    if (fields == 0)
    {
        DiscardUpdateMsg();
        return;
    }

    msg.WriteAt(fields_pos, fields);
}
