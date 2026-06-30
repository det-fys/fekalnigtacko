#pragma once

#include "utils/cmdlinestream.hpp"
#include <functional>
#include <map>

namespace game
{

class Player;

using CommandFlags = uint8_t;
enum CommandFlag : CommandFlags
{
    CMDF_NONE = 0,
    CMDF_ADMIN_ONLY = 1,
};

struct CommandData
{
    Player& player;
    CmdLineStream& line;

    CommandData(Player& player, CmdLineStream& line);

    void SendMessage(const std::string& text) const;
    void SendError(const std::string& text) const;
};

using CommandCallback = std::function<void(const CommandData&)>;

struct Command
{
    std::string name;
    std::string desc;
    CommandCallback cb;
    CommandFlags flags;
};

using ProcessCommandsCallback = std::function<void(const Command&)>;

class CommandList
{
public:
    CommandList();

    void RegisterCommand(std::string name, CommandFlags flags, std::string desc, CommandCallback cb);
    void ProcessCommads(ProcessCommandsCallback cb);
    void Execute(Player& player, CmdLineStream& line);

    static void SendError(Player& player, const std::string& msg);

private:
    void ExecuteCommand(Player& player, const Command& cmd, CmdLineStream& line);

private:
    std::map<std::string, Command> commands_;
};



}