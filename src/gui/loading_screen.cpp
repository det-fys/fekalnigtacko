#include "loading_screen.hpp"

#include <string>

void gui::DrawLoadingScreen(Context& ctx, int percent)
{
    float margin = 50.0f;
    glm::vec2 size(400.0f, 15.0f);
    glm::vec2 pos(margin, ctx.GetViewportSize().y - margin - size.y);

    float loaded = static_cast<float>(percent) * 0.01f;

    ctx.DrawRect(pos, pos + size, 0x77FFFFFF);
    ctx.DrawRect(pos, pos + glm::vec2(size.x * loaded, size.y), 0xFF00FFFF);

    std::string load_text = std::to_string(percent) + "%";
    ctx.DrawTextAligned(load_text, pos + glm::vec2(size.x + 50.0f, size.y * 0.5f), glm::vec2(-0.5f, -0.5f));
}
