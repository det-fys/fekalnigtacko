#include "controllable_character.hpp"

#include "drivable_vehicle.hpp"
#include "player.hpp"

namespace game
{

class OpenWorld;

class PlayerCharacter : public ControllableCharacter
{
public:
    using Super = ControllableCharacter;

    PlayerCharacter(World& world, OpenWorld& openworld, Player& player);

    virtual void Update() override;

    virtual void VehicleChanged() override;

    void ProcessInput(PlayerInputType type, bool enabled);

private:
    void UpdateInputs();
    void UpdateUseTarget();

private:
    OpenWorld& world_;
    Player& player_;
};


}