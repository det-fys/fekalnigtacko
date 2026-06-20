#include "player_character.hpp"

#include "world.hpp"
#include "input_mapping.hpp"
#include "utils/random.hpp"

game::PlayerCharacter::PlayerCharacter(World& world, Player& player, const HumanCharacterTuning& tuning) : Super(world, tuning), player_(&player)
{
    EnablePhysics(true);
    UpdatePlayerCamera();
    SetNametag(player.GetName());
    SendUseTargetInfo();

    // give some shit
    GiveItem(std::make_shared<ItemInstance>("airsniper"), false);
    GiveAmmo("pellet", 50);
    // GiveItem(std::make_shared<ItemInstance>("ak47"), false);
    // GiveItem(std::make_shared<ItemInstance>("uzi"), false);
}

void game::PlayerCharacter::Update()
{
    UpdateUseTarget();
    UpdateAimTarget();
    CheckItemSwitch();
    UpdateInputs();
    Super::Update();
    
    if (GetRideable() && IsDriver())
    {
        GetRideable()->SetRideableViewAngles(GetViewYaw(), GetViewPitch());
    }

    UpdateHudData();
}

void game::PlayerCharacter::ReceiveDamage(const DamageInfo& damage)
{
    if (!IsAlive())
        return;

    Super::ReceiveDamage(damage);

    if (player_)
    {
        player_->DisplayDamageEvent(DAMAGE_EVENT_RECEIVED);
    }

    if (!IsAlive())
    {
        // just died
        std::string_view killer_name;
        auto killer_character = dynamic_cast<PlayerCharacter*>(damage.inflictor);
        if (killer_character)
        {
            auto killer_player = killer_character->GetPlayer();
            if (killer_player)
            {
                killer_name = killer_player->GetName();
            }
        }

        SendDeathMessage(killer_name);
        SetNametag(std::string{});

    }
}

void game::PlayerCharacter::ProcessInput(PlayerInputType type, bool enabled)
{
    switch (type)
    {
    case IN_USE:
        UseChanged(enabled);
        break;

    default:
        UpdateInputs();
        break;
    }
}

void game::PlayerCharacter::DetachFromPlayer()
{
    player_ = nullptr;
}

void game::PlayerCharacter::SetInventory(std::unique_ptr<Inventory> inventory)
{
    if (inventory_)
    {
        TakeInventory();
    }

    inventory_ = std::move(inventory);
    UpdateHudSlots();

    // if (inventory_->active_slot > 0)
    // {
    //     SetWeaponSlot(inventory_->active_slot);
    // }
}

std::unique_ptr<game::Inventory> game::PlayerCharacter::TakeInventory()
{
    Equip(nullptr);
    auto inv = std::move(inventory_);
    UpdateHudSlots();
    return inv;
}

void game::PlayerCharacter::GiveItem(std::shared_ptr<ItemInstance> item, bool can_equip)
{
    EnsureInventory();

    size_t slot_idx = ((item->def->slot + 9) % 10);
    auto& slot = inventory_->slots[slot_idx];

    bool equip = can_equip && (!GetHeldItem() || GetHeldItem() == slot);

    if (!slot || slot->def->name != item->def->name)
    {
        // current item in the slot is other item or none
        slot = std::move(item);
    }
    else
    {
        // already have this, extract ammo
        GiveAmmo(item->def->ammo_type, item->ammo);
    }

    UpdateHudSlots();
    
    if (equip)
    {
        SetWeaponSlot(slot_idx);
    }
}

void game::PlayerCharacter::GiveAmmo(const std::string& ammo_name, size_t count)
{
    EnsureInventory();
    inventory_->ammo[ammo_name] += count;
}

void game::PlayerCharacter::OnDamageDealt(bool was_kill)
{
    if (!player_)
        return;

    player_->DisplayDamageEvent(was_kill ? DAMAGE_EVENT_DEALT_KILL : DAMAGE_EVENT_DEALT);
}

float game::PlayerCharacter::GetHitBoneDamageMultiplier(const std::string_view hitbone)
{
    return 0.25f * Super::GetHitBoneDamageMultiplier(hitbone);
}

void game::PlayerCharacter::OnRideableChanged()
{
    UpdatePlayerCamera();
    UpdateInputs();
}

void game::PlayerCharacter::OnAimingChanged()
{
    UpdatePlayerCamera();
}

void game::PlayerCharacter::OnHeldItemChanged()
{
    const auto& item = GetHeldItem();
    hud_data_.held_item = item ? item->def->name : "";
}

bool game::PlayerCharacter::HaveAmmo(const std::string& ammo_name)
{
    if (!inventory_)
        return false;
    
    auto it = inventory_->ammo.find(ammo_name);
    if (it == inventory_->ammo.end())
        return false;

    return it->second > 0;
}

size_t game::PlayerCharacter::GetAmmo(size_t required, const std::string& ammo_name)
{
    if (!inventory_)
        return 0;

    auto it = inventory_->ammo.find(ammo_name);
    if (it == inventory_->ammo.end())
        return 0;

    size_t give = glm::min(it->second, required);
    it->second -= give;

    return give;
}

void game::PlayerCharacter::SpawnLoot()
{
    if (!inventory_)
        return;

    for (auto& slot : inventory_->slots)
    {
        if (!slot)
            continue;

        size_t ammo = GetAmmo(slot->def->clip_size * 5, slot->def->ammo_type);
        auto pos = root_.GetGlobalPosition() + glm::vec3(RandomFloat(-1.0f, 1.0f), RandomFloat(-1.0f, 1.0f), RandomFloat(-0.1f, 1.0f));
        GetWorld().CreateItemPickup(pos, std::move(slot), RandomInt(59000, 61000), 0, ammo);
    }

    inventory_.reset();
}

void game::PlayerCharacter::UpdatePlayerCamera()
{
    if (!player_)
        return;

    CameraInfo camera_info{};
    camera_info.character_entnum = GetEntNum();
    camera_info.rideable_entnum = GetRideable() ? GetRideable()->GetEntity().GetEntNum() : 0;

    if (GetAiming())
        camera_info.flags |= CAM_AIMING;

    player_->SetCamera(camera_info);
}

void game::PlayerCharacter::UpdateInputs()
{
    auto in = (player_ && IsAlive()) ? player_->GetInput() : 0;
    
    if (auto rideable = GetRideable(); rideable)
    {
        SetInputs(0);

        auto rideable_in = in;

        if (IsDriver())
        {
            if (GetVehicle() && GetHeldItem() && GetHeldItem()->def->twohanded)
            {
                rideable_in &= ~((1 << IN_RIGHT) | (1 << IN_LEFT));
            }

            rideable->SetRideableInput(rideable_in);
        }
    }
    else
    {
        SetInputs(MapPlayerInputToCharacterInput(in));
    }

    SetAimHeld(in & (1 << IN_ATTACK_SECONDARY));
    SetFireHeld(in & (1 << IN_ATTACK_PRIMARY));
    SetReloadHeld(in & (1 << IN_RELOAD));
}

void game::PlayerCharacter::UpdateAimTarget()
{
    if (!player_ || !IsAlive())
        return;

    glm::vec3 eye, forward;
    if (!player_->GetView(eye, forward))
        return;

    auto target = eye + forward * 1000.0f;

    if (GetAiming()) // save perf if not aiming
    {
        GetWorld().TraceBullet(eye, target, this, target); // update target if hit
    }

    SetAimTarget(target);

    // GetWorld().Beam(eye, target, 0xFFFF00, 1.0 / 25.0f);
    // GetWorld().BeamBox(target - 0.05f, target + 0.05f, 0xFFFF00, 1.5f / 25.0f);
}

void game::PlayerCharacter::CheckItemSwitch()
{
    if (!player_ || !inventory_ || !IsAlive())
        return;
    
    auto in = player_->GetNewInput();

    if (in & (1 << IN_HOLSTER))
    {
        Equip(GetHeldItem() ? nullptr : inventory_->slots[inventory_->active_slot]);
        return;
    }

    for (size_t i = 0; i < 10; ++i) 
    {
        if ((in & (1 << (IN_WEAPON_1 + i))) == 0)
            continue;
    
        SetWeaponSlot(i);
        break;
    }
}

void game::PlayerCharacter::UpdateUseTarget()
{
    UseTargetQueryResult res{};
    auto new_use_target = IsAlive() ? world_.GetBestUseTarget(*this, res) : nullptr;

    if (new_use_target != use_target_ || res.enabled != use_enabled_ || res.error_text != use_error_ || res.delay != use_delay_)
    {
        use_target_ = new_use_target;
        use_enabled_ = res.enabled;
        use_delay_ = res.delay;
        use_error_ = res.error_text;
        use_progress_ = 0.0f;
        using_ = false;
        
        SendUseTargetInfo();
    }

    if (use_target_ && use_enabled_ && using_)
    {
        use_progress_ += 0.04f;
        if (use_progress_ >= use_delay_)
        {
            using_ = false;
            use_progress_ = 0.0f;
            use_target_->usable->Use(*this, use_target_->id);
        }
    }

}

void game::PlayerCharacter::UseChanged(bool enabled)
{
    if (!IsAlive())
        return;

    if (!use_target_)
    {
        // exit rideable if not target
        if (enabled && GetRideable())
            Ride(nullptr, 0);

        return;
    }

    use_progress_ = 0.0f;

    if (!use_enabled_)
        return;

    bool change = using_ != enabled;
    using_ = enabled;

    if (change)
        SendUseTargetInfo();

}

void game::PlayerCharacter::SendUseTargetInfo()
{
    if (!player_)
        return;

    if (!use_target_)
    {
        player_->SetUseTarget(std::string(), std::string(), 0.0f);
        return;
    }

    std::string error_text;
    if (use_error_)
        error_text = use_error_;

    player_->SetUseTarget(use_target_->desc, error_text, using_ ? use_delay_ - use_progress_ : 0.0f);
}

void game::PlayerCharacter::EnsureInventory()
{
    if (!inventory_)
    {
        inventory_ = std::make_unique<Inventory>();
    }
}

void game::PlayerCharacter::SetWeaponSlot(size_t slot)
{ 
    if (!inventory_ || !inventory_->slots[slot])
        return;
    
    inventory_->active_slot = slot;
    Equip(inventory_->slots[slot]);
}

void game::PlayerCharacter::UpdateHudData() 
{
    if (!player_)
        return;

    // general
    hud_data_.health = static_cast<uint8_t>(GetHealth());

    // item
    const auto& item = GetHeldItem();
    hud_data_.ammo_loaded = item ? item->ammo : 0;

    hud_data_.ammo_total = 0;
    if (item && inventory_)
    {
        auto it = inventory_->ammo.find(item->def->ammo_type);
        if (it != inventory_->ammo.end())
        {
            hud_data_.ammo_total = it->second;
        }
    }

    // death
    hud_data_.dead = IsDead() ? 1 : 0;

    player_->SetHudData(hud_data_);
}

void game::PlayerCharacter::UpdateHudSlots()
{
    hud_data_.weapon_slots = 0;

    if (!inventory_ || IsDead())
        return;

    for (size_t i = 0; i < 10; ++i)
    {
        if (inventory_->slots[i])
            hud_data_.weapon_slots |= 1 << i;
    }


}

static std::string_view GetRandomDeathMessageFormat()
{
    switch (rand() % 8)
    {
    case 1:
        return "byl zneškodněn";
    case 2:
        return "chcíp";
    case 3:
        return "umříl";
    case 4:
        return "umřel";
    case 5:
        return "pošel";
    case 6:
        return "odešel na věčné časy";
    case 7:
        return "už není mezi námi";
    default:
        return "zesnul";
    }
}

static std::string_view GetRandomDeathMessageFormatKilled()
{
    switch (rand() % 8)
    {
    case 0:
        return "zneškodnil";
    case 1:
        return "vyřešil";
    case 2:
        return "zajebal";
    case 3:
        return "zlikvidoval";
    case 4:
        return "odstranil";
    case 5:
        return "terminoval";
    case 6:
        return "zabil";
    default:
        return "kilnul";
    }
}

void game::PlayerCharacter::SendDeathMessage(std::string_view killer_name)
{
    if (!player_)
        return;

    std::string message;

    if (killer_name.empty())
    {
        message += player_->GetName();
        message += "^r ";
        message += GetRandomDeathMessageFormat();
    }
    else
    {
        message += killer_name;
        message += "^r ";
        message += GetRandomDeathMessageFormatKilled();
        message += " ";
        message += player_->GetName();
    }

    GetWorld().SendChat(message);

}
