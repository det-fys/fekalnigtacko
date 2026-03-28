#include "remote_menu_view.hpp"
#include "game/remote_menu.hpp"
#include "client_session.hpp"

game::view::RemoteMenuView::RemoteMenuView(ClientSession& session, net::MenuId id) : session_(session), id_(id) {}

bool game::view::RemoteMenuView::ProcessMessage(net::MenuMessageType type, net::InMessage& msg)
{
    switch (type)
    {
    case net::MMSG_UPDATE:
        return ProcessUpdateMsg(msg);
    case net::MMSG_ITEM_UPDATE_TEXT:
        return ProcessItemUpdateTextMsg(msg);
    case net::MMSG_ITEM_UPDATE_SELECTION:
        return ProcessItemUpdateSelectionMsg(msg);
    default:
        return false;
    }
}

bool game::view::RemoteMenuView::ProcessUpdateMsg(net::InMessage& msg)
{
    net::MenuTitle title;
    net::MenuItemCount itemcount;

    if (!msg.Read(title) || !msg.Read(itemcount))
        return false;

    SetTitle(title);
    Clear();

    for (net::MenuItemId i = 0; i < itemcount; ++i)
    {
        game::RemoteMenuItemType type;
        net::MenuItemText text;
        net::MenuItemSelection selection;

        if (!msg.Read(type) || !msg.Read(text) || !msg.Read(selection))
            return false;

        switch (type)
        {
            case RM_BUTTON:
            {
                auto& btn = Add<gui::ButtonMenuItem>(text);
                btn.SetClickCallback([this, i] { OnItemClick(i); });
                break;
            }
            
            case RM_SELECT:
            {
                auto& select = Add<gui::SelectMenuItem>(text);
                select.SetSelectionText(selection);
                select.SetClickCallback([this, i] { OnItemClick(i); });
                select.SetSwitchCallback([this, i] (int dir) { OnItemSelectionChange(i, dir); });
                break;
            }

            default:
                break;
        }

    }


    return true;
}

bool game::view::RemoteMenuView::ProcessItemUpdateTextMsg(net::InMessage& msg)
{
    net::MenuItemId idx;
    net::MenuItemText text;

    if (!msg.Read(idx) || !msg.Read(text))
        return false;

    if (idx >= GetNumItems())
        return false;

    auto& item = GetItem(idx);

    if (auto btn = dynamic_cast<gui::ButtonMenuItem*>(&item); btn)
    {
        btn->SetText(text);
    }
    else
    {
        return false;
    }

    return true;
}

bool game::view::RemoteMenuView::ProcessItemUpdateSelectionMsg(net::InMessage& msg)
{
    net::MenuItemId idx;
    net::MenuItemSelection selection;

    if (!msg.Read(idx) || !msg.Read(selection))
        return false;

    if (idx >= GetNumItems())
        return false;

    auto& item = GetItem(idx);

    auto select = dynamic_cast<gui::SelectMenuItem*>(&item);
    if (!select)
        return false;

    select->SetSelectionText(selection);
    return true;
}

void game::view::RemoteMenuView::OnItemClick(net::MenuItemId idx)
{
    auto msg = BeginActionMsg(net::MA_CLICK);
    msg.Write(idx);
}

void game::view::RemoteMenuView::OnItemSelectionChange(net::MenuItemId idx, int dir)
{
    if (dir == 0)
        return;

    auto msg = BeginActionMsg(net::MA_SELECT);
    msg.Write(idx);
    msg.Write<net::MenuSelectDir>(dir < 0 ? 0 : 1);
}

void game::view::RemoteMenuView::OnFocusChanged()
{
    auto msg = BeginActionMsg(net::MA_HOVER);
    msg.Write<net::MenuItemId>(GetFocusedItemIndex());
}

net::OutMessage game::view::RemoteMenuView::BeginActionMsg(net::MenuActionType type)
{
    auto msg = session_.BeginMsg(net::MSG_MENUACTION);
    msg.Write(id_);
    msg.Write(type);
    return msg;
}
