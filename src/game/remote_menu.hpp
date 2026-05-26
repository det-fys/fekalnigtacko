#pragma once

#include <string>
#include <functional>
#include <memory>
#include "net/defs.hpp"
#include "net/msg_producer.hpp"
#include "net/outmessage.hpp"
#include "net/inmessage.hpp"

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
using RemoteMenuExitCallback = std::function<void()>;

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
    RemoteMenu(net::MenuId id, std::string title);

    RemoteMenuItem& AddItem(RemoteMenuItemType type, std::string text);

    net::MenuId GetId() const { return id_; }

    bool ProcessActionMsg(net::InMessage& msg, net::MenuActionType type);

    void SetHoveredIdx(int hovered);

    void SetOnExit(RemoteMenuExitCallback on_exit) { on_exit_ = std::move(on_exit); }

    void Update();

private:
    net::OutMessage BeginMenuMsg(net::MenuMessageType type);

    // action handlers
    bool ProcessClickMsg(net::InMessage& msg);
    bool ProcessSelectMsg(net::InMessage& msg);
    bool ProcessHoverMsg(net::InMessage& msg);
    bool ProcessExitMsg(net::InMessage& msg);

private:
    net::MenuId id_;
    std::string title_;
    bool synced_ = false;
    bool items_synced_ = false;
    std::vector<std::unique_ptr<RemoteMenuItem>> items_;
    int hovered_ = 0;

    RemoteMenuExitCallback on_exit_;

    friend class RemoteMenuItem;
};

}