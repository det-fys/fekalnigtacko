#pragma once

#include <vector>
#include <concepts>

#include "context.hpp"

namespace gui
{

enum MenuInput
{
    MI_UP,
    MI_DOWN,
    MI_LEFT,
    MI_RIGHT,
    MI_BACK,
    MI_ENTER,
};

class MenuItem;

struct DrawMenuItemArgs
{
    Context& ctx;
    glm::vec2 pos;
    bool focused;

    DrawMenuItemArgs(Context& ctx, const glm::vec2& pos, bool focused) : ctx(ctx), pos(pos), focused(focused) {}
};

class Menu
{
public:
    Menu() = default;

    template <std::derived_from<MenuItem> T, typename... TArgs>
    T& Add(TArgs&&... args)
    {
        auto item = std::make_unique<T>(std::forward<TArgs>(args)...);
        auto& item_ref = *item;
        items_.emplace_back(std::move(item));
        return item_ref;
    }
    
    void Draw(Context& ctx, const glm::vec2& pos) const;
    void Input(MenuInput in);

    glm::vec2 MeasureSize() const;

private:
    void SwitchFocus(int dir);

private:
    std::vector<std::unique_ptr<MenuItem>> items_;
    size_t focus_ = 0;

};

class MenuItem
{
public:
    constexpr static uint32_t COLOR_INACTIVE = 0xFFFFFFFF;
    constexpr static uint32_t COLOR_FOCUSED = 0xFF00FFFF;

    MenuItem() = default;

    virtual void Draw(const DrawMenuItemArgs& args) const {}
    virtual void Input(MenuInput in) {}

    const glm::vec2& GetSize() const { return size_; }

    virtual ~MenuItem() = default;

protected:
    glm::vec2 size_;

};

class ButtonMenuItem : public MenuItem
{
public:
    using Super = MenuItem;

    ButtonMenuItem(std::string text);

    virtual void Draw(const DrawMenuItemArgs& args) const override;
    virtual void Input(MenuInput in) override;

    void SetClickCallback(std::function<void()> click_cb);

    virtual ~ButtonMenuItem() = default;

private:
    std::string text_;
    std::function<void()> click_cb_;
};

class SelectMenuItem : public ButtonMenuItem
{
public:
    using Super = ButtonMenuItem;

    SelectMenuItem(std::string text);

    virtual void Draw(const DrawMenuItemArgs& args) const override;
    virtual void Input(MenuInput in) override;

    void SetSwitchCallback(std::function<void(int)> switch_cb);
    void SetSelectionText(std::string select_text) { select_text_ = std::move(select_text); }

    virtual ~SelectMenuItem() = default;

protected:
    std::string select_text_;
    std::function<void(int)> switch_cb_;

};



}