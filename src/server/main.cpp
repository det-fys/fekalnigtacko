#include <iostream>
#include <stdexcept>

#include "server.hpp"

int main()
{
    srand(time(NULL));
    
    try
    {
        sv::Server server(11200);
        server.Run();
    } catch (const std::exception& e)
    {
        std::cerr << "FATAL ERROR: " << e.what() << std::endl;
        return 1;
    } 

    return 0;
}
