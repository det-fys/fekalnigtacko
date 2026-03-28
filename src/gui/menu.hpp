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
    glm::vec2 size;
    bool focused;

    DrawMenuItemArgs(Context& ctx) : ctx(ctx) {}
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
    
    void Clear();

    void Draw(Context& ctx, const glm::vec2& pos) const;
    void Input(MenuInput in);

    void SetTitle(std::string title);
    void SetItemSize(const glm::vec2& itemsize) { itemsize_ = itemsize;  }
    
    size_t GetFocusedItemIndex() const { return focus_; }
    
    size_t GetNumItems() const { return items_.size(); }
    MenuItem& GetItem(size_t idx) const { return *items_[idx]; }
    
    glm::vec2 MeasureSize() const;

protected:
    virtual void OnFocusChanged() {}

private:
    void SwitchFocus(int dir);

private:
    std::string title_;
    std::vector<std::unique_ptr<MenuItem>> items_;
    glm::vec2 itemsize_ = glm::vec2(300.0f, 40.0f);
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
    glm::vec2 size_ = glm::vec2(0.0f);

};

class ButtonMenuItem : public MenuItem
{
public:
    using Super = MenuItem;

    ButtonMenuItem(std::string text);

    virtual void Draw(const DrawMenuItemArgs& args) const override;
    virtual void Input(MenuInput in) override;

    void SetClickCallback(std::function<void()> click_cb);

    void SetText(std::string text) { text_ = std::move(text); }

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