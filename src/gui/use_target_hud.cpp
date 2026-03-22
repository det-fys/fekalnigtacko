#include "use_target_hud.hpp"

gui::UseTargetHud::UseTargetHud(const float& time) : time_(time) {}

void gui::UseTargetHud::SetData(std::string text, std::string error_text, float delay)
{
    text_ = std::move(text);

    if (error_text.empty())
        error_text_.clear();
    else
        error_text_ = "(" + error_text + ")";

    start_time_ = time_;
    end_time_ = delay > 0.01f ? start_time_ + delay : start_time_;
}

void gui::UseTargetHud::Draw(Context& ctx) const
{
    if (text_.empty())
        return;

    bool active = start_time_ != end_time_;
    uint32_t text_color = (!error_text_.empty()) ? 0xFFCCCCCC : (active ? 0xFF00FFFF : 0xFFFFFFFF);

    const float spacing = 10.0f;
    glm::vec2 key_size(30.0f);
    glm::vec2 text_size = ctx.MeasureText(text_);
    float total_width = key_size.x + spacing + text_size.x;
    
    glm::vec2 error_size(0.0f);
    if (!error_text_.empty())
    {
        error_size = ctx.MeasureText(error_text_);
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
    ctx.DrawText(text_, text_p, text_color);
    
    x += text_size.x + spacing;
    
    // draw error text
    if (!error_text_.empty())
    {
        glm::vec2 error_text_p(x, center_y - error_size.y * 0.5f);
        ctx.DrawText(error_text_, error_text_p, 0xFF7777FF);
    
        x += error_size.x + spacing;
    }

    // draw progress bar
    if (active)
    {
        float t = (time_ - start_time_) / (end_time_ - start_time_); 
        t = glm::clamp(t, 0.0f, 1.0f);

        glm::vec2 progress_p0(x, center_y - progress_size.y * 0.5f);
        glm::vec2 progress_p1 = progress_p0 + progress_size;
        glm::vec2 progress_p1_bar = progress_p0 + glm::vec2(t * progress_size.x, progress_size.y);
        
        ctx.DrawRect(progress_p0, progress_p1, 0x77000000);
        ctx.DrawRect(progress_p0, progress_p1_bar, 0xFF00FFFF);
    }
}
