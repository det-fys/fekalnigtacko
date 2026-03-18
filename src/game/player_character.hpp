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

    PlayerCharacter(World& world, Player& player);

    virtual void Update() override;

    virtual void VehicleChanged() override;

    void ProcessInput(PlayerInputType type, bool enabled);

private:
    void UpdateInputs();
    void UpdateUseTarget();

private:
    Player& player_;
};


}