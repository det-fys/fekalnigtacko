#include <iostream>
#include <stdexcept>

#include "server.hpp"
#include "server_cfg.hpp"
#include "utils/cvars.hpp"

CVAR(uint16_t, sv_port, CV_DEFAULT, 11200);

int main()
{
    srand(time(NULL));
    
    try
    {
        sv::LoadCfg();
        sv::Server server(sv_port.Get());
        server.Run();
    } catch (const std::exception& e)
    {
        std::cerr << "FATAL ERROR: " << e.what() << std::endl;
        return 1;
    } 

    return 0;
}
