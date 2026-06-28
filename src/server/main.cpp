#include <iostream>
#include <stdexcept>

#include "server.hpp"
#include "server_cfg.hpp"

int main()
{
    srand(time(NULL));
    
    try
    {
        sv::LoadCfg();
        sv::Server server(sv::GetCfg().port);
        server.Run();
    } catch (const std::exception& e)
    {
        std::cerr << "FATAL ERROR: " << e.what() << std::endl;
        return 1;
    } 

    return 0;
}
