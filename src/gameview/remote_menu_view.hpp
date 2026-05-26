#pragma once

#include "gui/menu.hpp"
// #include "net/msg_producer.hpp"
#include "net/inmessage.hpp"
#include "net/outmessage.hpp"
#include "net/defs.hpp"

namespace game::view
{

class ClientSession;

class RemoteMenuView : public gui::Menu
{
public:
    using Super = gui::Menu;

    RemoteMenuView(ClientSession& session, net::MenuId id);

    bool ProcessMessage(net::MenuMessageType type, net::InMessage& msg);

    net::MenuId GetId() const { return id_; }

private:
    bool ProcessUpdateMsg(net::InMessage& msg);
    bool ProcessItemUpdateTextMsg(net::InMessage& msg);
    bool ProcessItemUpdateSelectionMsg(net::InMessage& msg);
    
    void OnItemClick(net::MenuItemId idx);
    void OnItemSelectionChange(net::MenuItemId idx, int dir);
    
protected:
    void OnFocusChanged() override;
    void OnExit() override;

    net::OutMessage BeginActionMsg(net::MenuActionType type);
    
private:
    ClientSession& session_;
    net::MenuId id_;



};

}