#include "db.hpp"

std::string_view db::GetResultDescription(QueryResult res)
{
    switch (res)
    {
    case QR_OK:
        return "ok";
    case QR_NOT_FOUND:
        return "nenalezeno";
    case QR_INSUFFICIENT_FUNDS:
        return "to ti tvá momentální finanční situace neumožňuje";
    case QR_DB_ERROR:
        return "chyba databáze";
    default:
        return "jiná chyba";
    }
}
