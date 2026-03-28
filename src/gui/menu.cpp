#include "menu.hpp"

#include <cstring>

// Menu

static constexpr float menu_title_height = 50.0f;

void gui::Menu::Clear()
{
    items_.clear();
    focus_ = 0;
}

void gui::Menu::Draw(Context& ctx, const glm::vec2& pos) const
{
    // background
    auto size = MeasureSize();
    ctx.DrawRect(pos, pos + size, 0x88000000);
    
    // draw title
    glm::vec2 title_size(size.x, menu_title_height);
    ctx.DrawRect(pos, pos + title_size, 0x55000000);
    ctx.DrawTextAligned(title_, pos + title_size * 0.5f, glm::vec2(-0.5f));

    // draw items
    DrawMenuItemArgs args(ctx);
    args.size = itemsize_;
    args.pos.x = pos.x;

    for (size_t i = 0; i < items_.size(); ++i)
    {
        args.focused = focus_ == i;
        args.pos.y = pos.y + menu_title_height + static_cast<float>(i) * itemsize_.y;

        items_[i]->Draw(args);
    }
}

void gui::Menu::Input(MenuInput in)
{
    switch (in)
    {
    case MI_UP:
        SwitchFocus(-1);
        break;

    case MI_DOWN:
        SwitchFocus(1);
        break;

    default:
        if (!items_.empty())
            items_[focus_]->Input(in);
        break;
    }
}

void gui::Menu::SetTitle(std::string title)
{
    title_ = std::move(title);
}

glm::vec2 gui::Menu::MeasureSize() const
{
    return glm::vec2(itemsize_.x, menu_title_height + itemsize_.y * static_cast<float>(items_.size()));
}

void gui::Menu::SwitchFocus(int dir)
{
    if (items_.empty())
        return;

    size_t old_focus = focus_;

    if (items_.empty())
        focus_ = 0;
    else
        focus_ = (focus_ + items_.size() + dir) % items_.size();

    if (focus_ != old_focus)
    {
        OnFocusChanged();
    }
}

// ButtonMenuItem

gui::ButtonMenuItem::ButtonMenuItem(std::string text)
    : text_(std::move(text))
{
}

void gui::ButtonMenuItem::Draw(const DrawMenuItemArgs& args) const
{
    Super::Draw(args);
    glm::vec2 center = args.pos + glm::vec2(10.0f, args.size.y * 0.5f);
    args.ctx.DrawTextAligned(text_, center, glm::vec2(0.0f, -0.5f), args.focused ? COLOR_FOCUSED : COLOR_INACTIVE);
}

void gui::ButtonMenuItem::Input(MenuInput in)
{
    if (in == MI_ENTER && click_cb_)
        click_cb_();
}

void gui::ButtonMenuItem::SetClickCallback(std::function<void()> click_cb)
{
    click_cb_ = std::move(click_cb);
}

// SelectMenuItem

gui::SelectMenuItem::SelectMenuItem(std::string text)
    : ButtonMenuItem(std::move(text))
{
}

void gui::SelectMenuItem::Draw(const DrawMenuItemArgs& args) const
{
    Super::Draw(args);

    auto text_size = args.ctx.MeasureText(select_text_);

    glm::vec2 cursor = args.pos + glm::vec2(args.size.x - 10.0f, args.size.y * 0.5f);
    cursor.y -= text_size.y * 0.5f; // centered

    uint32_t text_color = args.focused ? COLOR_FOCUSED : COLOR_INACTIVE;
    uint32_t arrow_color = 0xFFFFFFFF;

    float arrow_width = 0.0f;
    if (args.focused)
    {
        arrow_width = args.ctx.MeasureText("<").x;
        cursor.x -= arrow_width;
        args.ctx.DrawText(">", cursor, arrow_color);
    }
    
    cursor.x -= text_size.x;
    args.ctx.DrawText(select_text_, cursor, text_color);
    
    if (args.focused)
    {
        cursor.x -= arrow_width;
        args.ctx.DrawText("<", cursor, arrow_color);
    }
}

void gui::SelectMenuItem::Input(MenuInput in)
{
    switch (in)
    {
    case MI_LEFT:
        if (switch_cb_)
            switch_cb_(-1);
        break;

    case MI_RIGHT:
        if (switch_cb_)
            switch_cb_(1);
        break;

    default:
        Super::Input(in);
        break;
    }
}

void gui::SelectMenuItem::SetSwitchCallback(std::function<void(int)> switch_cb)
{
    switch_cb_ = std::move(switch_cb);
}
