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

    void DetachFromPlayer();

    Player* GetPlayer() const { return player_; }

private:
    void UpdateInputs();
    
    void UpdateUseTarget();
    void UseChanged(bool enabled);
    void SendUseTargetInfo();

private:
    Player* player_;

    // use target
    const UseTarget* use_target_ = nullptr;
    bool use_enabled_ = false;
    float use_delay_ = 0.0f;
    const char* use_error_ = nullptr;
    bool using_ = false; // not drugs lol
    float use_progress_ = 0.0f;
};


}