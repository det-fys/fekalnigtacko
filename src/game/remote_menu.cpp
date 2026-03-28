#include "remote_menu.hpp"
#include "net/defs.hpp"

game::RemoteMenuItem::RemoteMenuItem(RemoteMenu& menu, RemoteMenuItemType type, std::string text) : menu_(menu), type_(type), text_(std::move(text))
{
}

void game::RemoteMenuItem::SetText(std::string text)
{
    if (text == text_)
        return;

    text_ = std::move(text);
    text_synced_ = false;
    menu_.items_synced_ = false;
}

void game::RemoteMenuItem::SetSelection(std::string selection)
{
    if (selection == selection_)
        return;
    
    selection_ = std::move(selection);
    selection_synced_ = false;
    menu_.items_synced_ = false;
}

game::RemoteMenu::RemoteMenu(net::MenuId id, std::string title) : id_(id), title_(std::move(title))
{
}

game::RemoteMenuItem& game::RemoteMenu::AddItem(RemoteMenuItemType type, std::string text)
{
    auto item = std::make_unique<RemoteMenuItem>(*this, type, std::move(text));
    auto& item_ref = *item;
    
    items_.push_back(std::move(item));
    synced_ = false;

    return item_ref;
}

bool game::RemoteMenu::ProcessActionMsg(net::InMessage& msg, net::MenuActionType type)
{
    switch (type)
    {
    case net::MA_CLICK:
        return ProcessClickMsg(msg);
    case net::MA_SELECT:
        return ProcessSelectMsg(msg);
    case net::MA_HOVER:
        return ProcessHoverMsg(msg);
    default:
        return false;
    }
}

void game::RemoteMenu::Update()
{
    if (synced_ && items_synced_)
        return;

    // not synced at all, write full update 
    if (!synced_)
    {
        auto msg = BeginMenuMsg(net::MMSG_UPDATE);
        msg.Write(net::MenuTitle(title_));
        msg.Write<net::MenuItemCount>(items_.size());
        for (auto& item : items_)
        {
            msg.Write(item->type_);
            msg.Write(net::MenuItemText(item->text_));
            msg.Write(net::MenuItemSelection(item->selection_));

            item->text_synced_ = true;
            item->selection_synced_ = true;
        }

        synced_ = true;
        items_synced_ = true;
        return;
    }

    // some items want sync
    if (!items_synced_)
    {
        for (size_t i = 0; i < items_.size(); ++i)
        {
            auto& item = *items_[i];

            if (!item.text_synced_)
            {
                auto msg = BeginMenuMsg(net::MMSG_ITEM_UPDATE_TEXT);
                msg.Write<net::MenuItemId>(i);
                msg.Write(net::MenuItemText(item.text_));
                item.text_synced_ = true;
            }

            if (!item.selection_synced_)
            {
                auto msg = BeginMenuMsg(net::MMSG_ITEM_UPDATE_SELECTION);
                msg.Write<net::MenuItemId>(i);
                msg.Write(net::MenuItemSelection(item.selection_));
                item.selection_synced_ = true;
            }
        }

        items_synced_ = true;
    }
}

net::OutMessage game::RemoteMenu::BeginMenuMsg(net::MenuMessageType type)
{
    auto msg = BeginMsg(net::MSG_REMOTEMENU);
    msg.Write(id_);
    msg.Write(type);
    return msg;
}

bool game::RemoteMenu::ProcessClickMsg(net::InMessage& msg)
{
    net::MenuItemId id;

    if (!msg.Read(id))
        return false;

    if (id > items_.size())
        return true; // not illegal

    auto& cb = items_[id]->on_click_;
    if (cb)
        cb();

    return true;
}

bool game::RemoteMenu::ProcessSelectMsg(net::InMessage& msg)
{
    net::MenuItemId id;
    net::MenuSelectDir dir;

    if (!msg.Read(id) || !msg.Read(dir))
        return false;

    if (id > items_.size())
        return true; // not illegal

    int idir = dir ? 1 : -1;

    auto& cb = items_[id]->on_select_;
    if (cb)
        cb(idir);

    return true;
}

bool game::RemoteMenu::ProcessHoverMsg(net::InMessage& msg)
{
    net::MenuItemId id;

    if (!msg.Read(id))
        return false;

    if (id > items_.size())
        return true; // not illegal

    if (id == hovered_)
        return true; // already hovered

    hovered_ = id;
    
    auto& cb = items_[id]->on_hovered_;
    if (cb)
        cb();

    return true;
}
