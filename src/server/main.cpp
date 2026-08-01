#include <iostream>
#include <stdexcept>

#include "server.hpp"
#include "server_cfg.hpp"
#include "utils/cvars.hpp"
#include "net/server_ws_crow.hpp"
#include "db/db_memory.hpp"
#include "version.hpp"

CVAR(uint16_t, sv_port, CV_CONST, 11200);

int main()
{
    std::cout << "Starting server " FEKAL_VERSION << std::endl;

    srand(time(NULL));
    
    try
    {
        sv::LoadCfg("server.cfg");
    
        // setup game
        game::GameInfo g_info{};
        g_info.db = std::make_unique<db::MemoryGameDatabase>();
        auto game = std::make_unique<game::Game>(std::move(g_info));

        // setup server
        sv::ServerInfo sv_info{};
        sv_info.iface = std::make_unique<net::CrowWSServerInterface>(sv_port.Get());
        sv_info.game = std::move(game);
        sv::Server server(std::move(sv_info));

        // run server
        server.Run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "FATAL ERROR: " << e.what() << std::endl;
        return 1;
    } 

    return 0;
}
