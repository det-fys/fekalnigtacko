#include "commands.hpp"
#include "player.hpp"
#include "utils/chatcolors.hpp"

game::CommandList::CommandList() {}

void game::CommandList::RegisterCommand(std::string name, CommandFlags flags, std::string desc, CommandCallback cb)
{
    auto& cmd = commands_[name];
    cmd.name = std::move(name);
    cmd.flags = flags;
    cmd.desc = std::move(desc);
    cmd.cb = std::move(cb);
}

void game::CommandList::ProcessCommads(ProcessCommandsCallback cb)
{
    for (const auto& cmd : commands_)
    {
        cb(cmd.second);
    }
}

void game::CommandList::Execute(Player& player, CmdLineStream& line)
{
    std::string cmd;
    if (!line.Read(cmd))
    {
        SendError(player, "není tam příkaz blbečku");
        return;
    }

    auto it = commands_.find(cmd);
    if (it == commands_.end())
    {
        SendError(player, "neznámej příkaz");
        return;
    }

    ExecuteCommand(player, it->second, line);
}

void game::CommandList::SendError(Player& player, const std::string& msg)
{
    player.SendChat(COL_ERROR "chyba: " + msg);
}

void game::CommandList::ExecuteCommand(Player& player, const Command& cmd, CmdLineStream& line)
{
    // TODO: check permissions
    if ((cmd.flags & CMDF_ADMIN_ONLY) && !player.IsAdmin())
    {
        SendError(player, "nejsi admin");
        return;
    }

    CommandData cmddata(player, line);
    cmd.cb(cmddata);
}

game::CommandData::CommandData(Player& player, CmdLineStream& line) : player(player), line(line) {}

void game::CommandData::SendMessage(const std::string& text) const
{
    player.SendChat(text);
}

void game::CommandData::SendError(const std::string& text) const
{
    CommandList::SendError(player, text);
}

