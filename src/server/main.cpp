#include <iostream>
#include <stdexcept>

#include "server.hpp"
#include "server_cfg.hpp"
#include "utils/cvars.hpp"
#include "net/server_ws_crow.hpp"

CVAR(uint16_t, sv_port, CV_CONST, 11200);

int main()
{
    srand(time(NULL));
    
    try
    {
        sv::LoadCfg("server.cfg");
        
        auto ws = std::make_unique<net::CrowWSServerInterface>(sv_port.Get());
        sv::Server server(std::move(ws));
        server.Run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "FATAL ERROR: " << e.what() << std::endl;
        return 1;
    } 

    return 0;
}
