#include "menu.hpp"

#include <cstring>

// Menu

void gui::Menu::Draw(Context& ctx, const glm::vec2& pos) const
{
    // background
    auto size = MeasureSize();
    ctx.DrawRect(pos, pos + size, 0x55000000);

    glm::vec2 cursor = pos;
    for (size_t i = 0; i < items_.size(); ++i)
    {
        items_[i]->Draw(DrawMenuItemArgs(ctx, cursor, focus_ == i));
        cursor.y += items_[i]->GetSize().y;
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

glm::vec2 gui::Menu::MeasureSize() const
{
    glm::vec2 size(0.0f);

    for (const auto& item : items_)
    {
        const auto& itemsize = item->GetSize();
        size.x = glm::max(size.x, itemsize.x);
        size.y += itemsize.y;
    }

    return size;
}

void gui::Menu::SwitchFocus(int dir)
{
    if (items_.empty())
        return;

    focus_ = (focus_ + items_.size() + dir) % items_.size();
}

// ButtonMenuItem

gui::ButtonMenuItem::ButtonMenuItem(std::string text)
    : text_(std::move(text))
{
    size_ = glm::vec2(300.0f, 30.0f);
}

void gui::ButtonMenuItem::Draw(const DrawMenuItemArgs& args) const
{
    Super::Draw(args);
    glm::vec2 center = args.pos + glm::vec2(0.0f, size_.y * 0.5f);
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

    char buffer[128];
    size_t len = snprintf(buffer, sizeof(buffer), "< %s >", select_text_.c_str());

    glm::vec2 center_right = args.pos + glm::vec2(size_.x, size_.y * 0.5f);
    args.ctx.DrawTextAligned(std::string_view(buffer, len), center_right, glm::vec2(-1.0f, -0.5f),
                             args.focused ? COLOR_FOCUSED : COLOR_INACTIVE);
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
