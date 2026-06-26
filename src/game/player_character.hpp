#include "human_character.hpp"

#include "drivable_vehicle.hpp"
#include "player.hpp"
#include "world.hpp"
#include "inventory.hpp"

namespace game
{

class PlayerCharacter : public HumanCharacter
{
public:
    using Super = HumanCharacter;

    PlayerCharacter(World& world, Player& player, const HumanCharacterTuning& tuning);

    virtual void Update() override;

    virtual void ReceiveDamage(const DamageInfo& damage) override;

    void ProcessInput(PlayerInputType type, bool enabled);

    void DetachFromPlayer();

    Player* GetPlayer() const { return player_; }

    void SetInventory(std::unique_ptr<Inventory> inventory);
    std::unique_ptr<Inventory> TakeInventory();

    void GiveItem(std::shared_ptr<ItemInstance> item, bool can_equip = true);
    void GiveAmmo(const std::string& ammo_name, size_t count);

    virtual void OnDamageDealt(bool was_kill) override;

    protected:
    virtual float GetHitBoneDamageMultiplier(const std::string_view hitbone) override;
    
    virtual void OnRideableChanged() override;
    virtual void OnAimingChanged() override;
    virtual void OnHeldItemChanged() override;
    virtual bool HaveAmmo(const std::string& ammo_name) override;
    virtual size_t GetAmmo(size_t required, const std::string& ammo_name) override;
    virtual void SpawnLoot() override;

private:
    void UpdatePlayerCamera();
    void UpdateInputs();
    void UpdateAimTarget();
    void CheckItemSwitch();
    
    void UpdateUseTarget();
    void UseChanged(bool enabled);
    void SendUseTargetInfo();

    void EnsureInventory();
    void SetWeaponSlot(size_t slot);

    void UpdateHudData();
    void UpdateHudSlots();

    void SendDeathMessage(std::string_view killer_name);

private:
    Player* player_;

    // use target
    const UseTarget* use_target_ = nullptr;
    bool use_enabled_ = false;
    float use_delay_ = 0.0f;
    const char* use_error_ = nullptr;
    bool using_ = false; // not drugs lol
    float use_progress_ = 0.0f;
    bool sprintheld_ = false;

    std::unique_ptr<Inventory> inventory_;

    PlayerHudData hud_data_;
};


}