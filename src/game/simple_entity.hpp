#include "entity.hpp"
#include "simple_entity_sync.hpp"

namespace game
{

class SimpleEntity : public Entity
{
public:
    using Super = Entity;

    SimpleEntity(World& world, const std::string& modelname);

    virtual void SendInitData(Player& player, net::OutMessage& msg) const override;
    virtual void Update() override;

    virtual void UpdatePreSync() {}

private:
    void UpdateSyncState();
    SimpleEntitySyncFieldFlags WriteState(net::OutMessage& msg, const SimpleEntitySyncState& base) const;
    void SendUpdateMsg();

private:
    std::string modelname_;

    SimpleEntitySyncState sync_[2];
    size_t sync_current_ = 0;

};

} // namespace game