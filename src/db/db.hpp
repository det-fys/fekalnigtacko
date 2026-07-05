#pragma once

#include <cstdint>
#include <string_view>
#include <string>

namespace db
{

using Id = uint32_t;
using PlayerId = Id;

using MoneyAmount = int64_t;

enum QueryResult
{
    QR_OK = 0,

    QR_NOT_FOUND,
    QR_INSUFFICIENT_FUNDS,
    QR_DB_ERROR,
    QR_OTHER,
};

class GameDatabase
{
public:
    virtual QueryResult VerifyPlayer(std::string_view token, PlayerId& out_player_id) = 0;
    virtual QueryResult GetPlayerName(PlayerId player_id, std::string& out_name) = 0;

    virtual QueryResult GetPlayerBalance(PlayerId player_id, MoneyAmount& out_balance) = 0;
    virtual QueryResult ChangePlayerBalance(PlayerId player_id, MoneyAmount delta) = 0;

    virtual ~GameDatabase() = default;
};

std::string_view GetResultDescription(QueryResult res);

} // namespace db
