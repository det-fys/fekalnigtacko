#pragma once

#include <string>

#include "context.hpp"

namespace gui
{

class PlayerHud
{
public:
    PlayerHud(const float& time);

    void SetHealth(float health) { health_ = health; }
    void SetWeaponSlots(uint8_t slots);
    void SetItemInfo(std::string item_name, size_t slot, size_t clip_size);
    void SetLoadedAmmo(size_t loaded_ammo) { loaded_ammo_ = loaded_ammo; }
    void SetTotalAmmo(size_t total_ammo) { total_ammo_ = total_ammo; }

    void SetUseTargetData(std::string text, std::string error_text, float delay);

    void SetDisplayCrosshair(bool show) { display_crosshair_ = show; }
    void SetDisplayScope(bool show) { display_scope_ = show; }
    void ShowDamageReceived();
    void ShowDamageDealt(bool kill);

    void SetDead(bool dead) { dead_ = dead; }

    void Update(float delta_time);
    void Draw(Context& ctx) const;

private:
    void UpdateWeaponSlotsText();

    glm::vec4 GetCrosshairColor() const;

    void DrawPain(Context& ctx) const;
    void DrawCrosshair(Context& ctx) const;
    void DrawScope(Context& ctx) const;
    void DrawHealthBar(Context& ctx) const;
    void DrawItemInfo(Context& ctx) const;
    void DrawUseTarget(Context& ctx) const;
    void DrawDeathScreen(Context& ctx) const;

private:
    const float& time_; 
    
    // resources
    std::shared_ptr<const gfx::Texture> crosshair_texture_;
    std::shared_ptr<const gfx::Texture> scope_texture_;

    // general
    float health_ = 0.0f;

    // weapon slots
    uint8_t weapon_slots_ = 0;
    size_t current_slot_ = 0;
    std::string weapon_slots_text_;

    // held item
    std::string item_name_;
    size_t clip_size_ = 0;
    size_t loaded_ammo_ = 0;
    size_t total_ammo_ = 0;

    // use target
    std::string ut_text_;
    std::string ut_error_text_;
    float ut_start_time_ = 0.0f;
    float ut_end_time_ = 0.0f;

    // crosshair & events
    bool display_crosshair_ = false;
    bool display_scope_ = false;
    float damage_received_factor_ = 0.0f;
    float damage_dealt_factor_ = 0.0f;
    float damage_dealt_kill_factor_ = 0.0f;
    
    // death
    bool dead_ = false;

};


}