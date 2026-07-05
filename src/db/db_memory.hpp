#pragma once

#include "db.hpp"

#include <map>
#include <string>

namespace db
{

struct MemoryPlayerData
{
    std::string name;
    MoneyAmount balance = 0;
};

class MemoryGameDatabase : public GameDatabase
{
public:
    virtual QueryResult VerifyPlayer(std::string_view token, PlayerId& out_player_id) override;
    virtual QueryResult GetPlayerName(PlayerId player_id, std::string& out_name) override;

    virtual QueryResult GetPlayerBalance(PlayerId player_id, MoneyAmount& out_balance) override;
    virtual QueryResult ChangePlayerBalance(PlayerId player_id, MoneyAmount delta) override;

private:
    MemoryPlayerData* GetPlayerData(PlayerId player_id);

private:
    std::map<PlayerId, MemoryPlayerData> players_;
    std::map<std::string, PlayerId> name2id_;
    PlayerId last_id_ = 0;
};

}
