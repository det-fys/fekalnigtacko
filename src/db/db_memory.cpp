#include "db_memory.hpp"

#include "utils/allocnum.hpp"

db::QueryResult db::MemoryGameDatabase::VerifyPlayer(std::string_view token, PlayerId& out_player_id)
{
    auto name = std::string(token);

    auto it = name2id_.find(name);
    if (it != name2id_.end())
    {
        out_player_id = it->second;
        return QR_OK;
    }

    auto num = utils::AllocNum(players_, last_id_);
    if (num == 0)
    {
        return QR_OTHER;
    }

    // init player
    name2id_[name] = num;
    auto& data = players_[num];
    data.name = std::move(name);
    data.balance = 0;

    out_player_id = num;
    return QR_OK;
}

db::QueryResult db::MemoryGameDatabase::GetPlayerName(PlayerId player_id, std::string& out_name)
{
    auto data = GetPlayerData(player_id);
    if (!data)
    {
        return QR_NOT_FOUND;
    }

    out_name = data->name;
    return QR_OK;
}

db::QueryResult db::MemoryGameDatabase::GetPlayerBalance(PlayerId player_id, MoneyAmount& out_balance)
{
    auto data = GetPlayerData(player_id);
    if (!data)
    {
        return QR_NOT_FOUND;
    }

    out_balance = data->balance;
    return QR_OK;
}

db::QueryResult db::MemoryGameDatabase::ChangePlayerBalance(PlayerId player_id, MoneyAmount delta)
{
    if (delta == 0)
    {
        return QR_OK;
    }

    auto data = GetPlayerData(player_id);
    if (!data)
    {
        return QR_NOT_FOUND;
    }

    if (delta < 0 && data->balance + delta < 0)
    {
        return QR_INSUFFICIENT_FUNDS;
    }

    data->balance += delta;
    return QR_OK;
}

db::MemoryPlayerData* db::MemoryGameDatabase::GetPlayerData(PlayerId player_id)
{
    auto it = players_.find(player_id);
    if (it == players_.end())
        return nullptr;

    return &it->second;
}
