#include <format>

#include "player_hud.hpp"
#include "utils/math.hpp"

#include "assets/asset_manager.hpp"

static uint32_t COLOR_ACTIVE = 0xFF00FFFF;
static uint32_t COLOR_NORMAL = 0xFFFFFFFF;
static uint32_t COLOR_DISABLED = 0xFFCCCCCC;
static uint32_t COLOR_ERROR = 0xFF7777FF;

#define PREFIX_SEPARATOR "^aaa"
#define PREFIX_CLIPSIZE "^ccc"
#define PREFIX_AMMO_NORMAL "^fff"
#define PREFIX_AMMO_FULL "^9f9"
#define PREFIX_AMMO_EMPTY "^f77"

#define PREFIX_WEAPON_SLOT_INACTIVE "^888"
#define PREFIX_WEAPON_SLOT_ACTIVE "^fff"
#define PREFIX_WEAPON_SLOT_CURRENT "^ff0"

gui::PlayerHud::PlayerHud(const float& time) : time_(time)
{
    crosshair_texture_ = assets::AssetManager::GetInstance().Get<gfx::Texture>("crosshair");
    scope_texture_ = assets::AssetManager::GetInstance().Get<gfx::Texture>("scope");

    UpdateWeaponSlotsText();
}

void gui::PlayerHud::SetWeaponSlots(uint8_t slots)
{
    weapon_slots_ = slots;
    UpdateWeaponSlotsText();
}

void gui::PlayerHud::SetItemInfo(std::string item_name, size_t slot, size_t clip_size)
{
    item_name_ = std::move(item_name);
    clip_size_ = clip_size;
    current_slot_ = slot;
    UpdateWeaponSlotsText();
}

void gui::PlayerHud::SetUseTargetData(std::string text, std::string error_text, float delay)
{
    ut_text_ = std::move(text);

    if (error_text.empty())
        ut_error_text_.clear();
    else
        ut_error_text_ = "(" + error_text + ")";

    ut_start_time_ = time_;
    ut_end_time_ = delay > 0.01f ? ut_start_time_ + delay : ut_start_time_;
}

void gui::PlayerHud::ShowDamageReceived()
{
    damage_received_factor_ = glm::min(damage_received_factor_ + 0.2f, 0.5f);
}

void gui::PlayerHud::ShowDamageDealt(bool kill)
{
    (kill ? damage_dealt_kill_factor_ : damage_dealt_factor_) = 1.0f;
}

void gui::PlayerHud::Update(float delta_time)
{
    MoveToward(damage_received_factor_, 0.0f, 1.0f * delta_time);
    MoveToward(damage_dealt_factor_, 0.0f, 5.0f * delta_time);
    MoveToward(damage_dealt_kill_factor_, 0.0f, 2.0f * delta_time);
}

void gui::PlayerHud::Draw(Context& ctx) const
{
    DrawCrosshair(ctx);
    DrawScope(ctx);
    DrawPain(ctx);
    DrawHealthBar(ctx);
    DrawItemInfo(ctx);
    DrawUseTarget(ctx);
    DrawDeathScreen(ctx);
}

void gui::PlayerHud::UpdateWeaponSlotsText()
{
    weapon_slots_text_.clear();

    for (size_t slot = 0; slot < 10; ++slot)
    {
        weapon_slots_text_.push_back(' ');
        
        std::string_view prefix = PREFIX_WEAPON_SLOT_INACTIVE;
        
        if (weapon_slots_ & (1 << slot))
            prefix = PREFIX_WEAPON_SLOT_ACTIVE;
        
        if (current_slot_ == slot + 1)
            prefix = PREFIX_WEAPON_SLOT_CURRENT;

        weapon_slots_text_ += prefix;
        weapon_slots_text_ += std::to_string((slot + 1) % 10);
    }
}

glm::vec4 gui::PlayerHud::GetCrosshairColor() const
{
    glm::vec4 color(1.0f, 1.0f, 1.0f, 0.0f);
    if (display_crosshair_ || display_scope_)
        color.a = 1.0f;

    if (damage_dealt_factor_ > 0.01f)
    {
        color = glm::mix(color, glm::vec4(0.3f, 0.3f, 0.3f, 1.0f), damage_dealt_factor_);
    }

    if (damage_dealt_kill_factor_ > 0.01f)
    {
        color = glm::mix(color, glm::vec4(1.0f, 0.1f, 0.1f, 1.0f), damage_dealt_kill_factor_);
    }

    return color;
}

void gui::PlayerHud::DrawPain(Context& ctx) const
{
    if (damage_received_factor_ <= 0.01f)
        return;

    glm::vec4 color(1.0f, 0.3f, 0.3f, damage_received_factor_);
    ctx.DrawRect(glm::vec2(0.0f), ctx.GetViewportSize(), glm::packUnorm4x8(color));
}

void gui::PlayerHud::DrawCrosshair(Context& ctx) const
{
    if (display_scope_)
        return;

    auto color = GetCrosshairColor();
    if (color.a < 0.25f)
        return;

    float crosshair_size = 32.0f / ctx.GetScale();

    auto& viewport_size = ctx.GetViewportSize();

    auto p0 = glm::round(viewport_size * 0.5f - crosshair_size * 0.5f);
    auto p1 = p0 + crosshair_size;

    ctx.DrawRect(p0, p1, glm::packUnorm4x8(GetCrosshairColor()), crosshair_texture_.get());
}

void gui::PlayerHud::DrawScope(Context& ctx) const
{
    if (!display_scope_)
        return;

    glm::vec2 p0(0.0f);
    glm::vec2 size = ctx.GetViewportSize();

    if (size.x < size.y)
    {
        p0.y += (size.y - size.x) * 0.5f;
        size.y = size.x;
        ctx.DrawRect(glm::vec2(0.0f), glm::vec2(p0.x + size.x, p0.y + 2.0f), 0xFF000000);
        ctx.DrawRect(glm::vec2(p0.x, p0.y + size.y - 2.0f), ctx.GetViewportSize(), 0xFF000000);
    }
    else
    {
        p0.x += (size.x - size.y) * 0.5f;
        size.x = size.y;
        ctx.DrawRect(glm::vec2(0.0f), glm::vec2(p0.x + 2.0f, p0.y + size.y), 0xFF000000);
        ctx.DrawRect(glm::vec2(p0.x + size.x - 2.0f, p0.y), ctx.GetViewportSize(), 0xFF000000);
    }

    p0 = glm::floor(p0);
    size = glm::floor(size);

    // glm::vec2 p1 = p0 + size * 0.5f;
    glm::vec2 p2 = p0 + size;

    auto color = GetCrosshairColor();
    ctx.DrawRect(p0, p2, glm::packUnorm4x8(color), scope_texture_.get());
    
}

void gui::PlayerHud::DrawHealthBar(Context& ctx) const
{
    const float margin = 30.0f;
    const glm::vec2 size(100.0f, 20.0f);

    glm::vec2 p0(margin, ctx.GetViewportSize().y - margin - size.y);
    glm::vec2 p1 = p0 + size;
    ctx.DrawRect(p0, p1, 0x99000000); // bg
    
    glm::vec2 p1_bar = p0 + glm::vec2(size.x * health_ * 0.01f, size.y);
    ctx.DrawRect(p0, p1_bar, 0xDD00BB00); // bar
}

static std::string_view GetAmmoColor(size_t loaded, size_t clip_size)
{
    if (loaded == 0)
        return PREFIX_AMMO_EMPTY;

    if (loaded == clip_size)
        return PREFIX_AMMO_FULL;

    return PREFIX_AMMO_NORMAL;
}

void gui::PlayerHud::DrawItemInfo(Context& ctx) const
{
    const float margin = 30.0f;
    const float line_height = 30.0f;

    glm::vec2 cursor(ctx.GetViewportSize() - margin);

    ctx.DrawTextAligned(weapon_slots_text_, cursor, glm::vec2(-1.0f, -1.0f), COLOR_NORMAL);
    cursor.y -= line_height * 1.5f;

    if (!item_name_.empty())
    {
        std::string ammo_text =
            std::format("{}{}" PREFIX_SEPARATOR "/" PREFIX_CLIPSIZE "{}" PREFIX_SEPARATOR " | {}{}",
                        GetAmmoColor(loaded_ammo_, clip_size_), loaded_ammo_, clip_size_, GetAmmoColor(total_ammo_, 0), total_ammo_);

        ctx.DrawTextAligned(ammo_text, cursor, glm::vec2(-1.0f, -1.0f), COLOR_NORMAL);
        cursor.y -= line_height;
        ctx.DrawTextAligned(item_name_, cursor, glm::vec2(-1.0f, -1.0f), COLOR_ACTIVE);
        cursor.y -= line_height;
    }
}

void gui::PlayerHud::DrawUseTarget(Context& ctx) const
{
        if (ut_text_.empty())
        return;

    bool active = ut_start_time_ != ut_end_time_;
    uint32_t text_color = (!ut_error_text_.empty()) ? COLOR_DISABLED : (active ? COLOR_ACTIVE : COLOR_NORMAL);

    const float spacing = 10.0f;
    glm::vec2 key_size(30.0f);
    glm::vec2 text_size = ctx.MeasureText(ut_text_);
    float total_width = key_size.x + spacing + text_size.x;
    
    glm::vec2 error_size(0.0f);
    if (!ut_error_text_.empty())
    {
        error_size = ctx.MeasureText(ut_error_text_);
        total_width += spacing + error_size.x;
    }

    glm::vec2 progress_size(60.0f, 10.0f);
    if (active)
    {
        total_width += spacing + progress_size.x;
    }

    auto& viewport_size = ctx.GetViewportSize();
    float center_x = viewport_size.x * 0.5f;
    float center_y = viewport_size.y - 50.0f;
    float x = center_x - total_width * 0.5f;

    // draw key bg
    glm::vec2 bg_p0(x, center_y - key_size.y * 0.5f);
    glm::vec2 bg_p1 = bg_p0 + key_size;
    ctx.DrawRect(bg_p0, bg_p1, 0x77000000);
    
    // draw key text
    static constexpr std::string_view key_text = "E";
    ctx.DrawTextAligned(key_text, bg_p0 + key_size * 0.5f, glm::vec2(-0.5f, -0.5f), text_color);
    
    x += key_size.x + spacing;

    // draw text
    glm::vec2 text_p(x, center_y - text_size.y * 0.5f);
    ctx.DrawText(ut_text_, text_p, text_color);
    
    x += text_size.x + spacing;
    
    // draw error text
    if (!ut_error_text_.empty())
    {
        glm::vec2 error_text_p(x, center_y - error_size.y * 0.5f);
        ctx.DrawText(ut_error_text_, error_text_p, COLOR_ERROR);
    
        x += error_size.x + spacing;
    }

    // draw progress bar
    if (active)
    {
        float t = (time_ - ut_start_time_) / (ut_end_time_ - ut_start_time_); 
        t = glm::clamp(t, 0.0f, 1.0f);

        glm::vec2 progress_p0(x, center_y - progress_size.y * 0.5f);
        glm::vec2 progress_p1 = progress_p0 + progress_size;
        glm::vec2 progress_p1_bar = progress_p0 + glm::vec2(t * progress_size.x, progress_size.y);
        
        ctx.DrawRect(progress_p0, progress_p1, 0x77000000);
        ctx.DrawRect(progress_p0, progress_p1_bar, COLOR_ACTIVE);
    }
}

void gui::PlayerHud::DrawDeathScreen(Context& ctx) const
{
    if (!dead_)
        return;

    ctx.DrawTextAligned("si chcíp", ctx.GetViewportSize() * 0.5f, glm::vec2(-0.5f), 0xFFFFFFFF, 3.0f);
}
