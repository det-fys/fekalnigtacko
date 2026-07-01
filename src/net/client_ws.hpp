#pragma once

#ifdef EMSCRIPTEN
#include "client_ws_emscripten.hpp"

namespace net
{
using WSClientInterface = EmscriptenWSClientInterface;
}

#else // EMSCRIPTEN
#include "client_ws_easywsclient.hpp"

namespace net
{
using WSClientInterface = EasyWsClientWSClientInterface;
}

#endif // EMSCRIPTEN
