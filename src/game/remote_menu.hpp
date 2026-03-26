#pragma once

#include <string>
#include <functional>
#include <memory>
#include "net/defs.hpp"
#include "net/msg_producer.hpp"
#include "net/outmessage.hpp"

namespace game
{

enum RemoteMenuItemType : uint8_t
{
    RM_LABEL,
    RM_BUTTON,
    RM_SELECT,
};

class RemoteMenu;
class RemoteMenuItem;

using RemoteMenuClickCallback = std::function<void()>;
using RemoteMenuHoveredCallback = std::function<void()>;
using RemoteMenuSelectCallback = std::function<void(int)>;

class RemoteMenuItem
{
public:
    RemoteMenuItem(RemoteMenu& menu, RemoteMenuItemType type, std::string text);

    void SetText(std::string text);
    void SetSelection(std::string selection);

    void SetOnClick(RemoteMenuClickCallback on_click) { on_click_ = std::move(on_click); }
    void SetOnSelect(RemoteMenuSelectCallback on_select) { on_select_ = std::move(on_select); }
    void SetOnHovered(RemoteMenuHoveredCallback on_hovered) { on_hovered_ = std::move(on_hovered); }

private:
    RemoteMenu& menu_;
    RemoteMenuItemType type_ = RM_LABEL;
    std::string text_;
    bool text_synced_ = false;
    std::string selection_;
    bool selection_synced_ = false;

    RemoteMenuClickCallback on_click_;
    RemoteMenuSelectCallback on_select_;
    RemoteMenuHoveredCallback on_hovered_;
    
    friend class RemoteMenu;
};

class RemoteMenu : public net::MsgProducer
{
public:
    RemoteMenu(net::MenuId id);

    RemoteMenuItem& AddItem(RemoteMenuItemType type, std::string text);

    void Update();

private:
    net::OutMessage BeginMenuMsg(net::MenuMessageType type);

private:
    net::MenuId id_;
    bool synced_ = false;
    bool items_synced_ = false;
    std::vector<std::unique_ptr<RemoteMenuItem>> items_;
    int hovered_ = -1;

    friend class RemoteMenuItem;
};

}