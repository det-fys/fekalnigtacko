#include "controllable_character.hpp"

#include "drivable_vehicle.hpp"
#include "player.hpp"
#include "world.hpp"

namespace game
{

class PlayerCharacter : public ControllableCharacter
{
public:
    using Super = ControllableCharacter;

    PlayerCharacter(World& world, Player& player, const CharacterTuning& tuning);

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