#pragma once

#ifdef EMSCRIPTEN

#include "client.hpp"

namespace net
{

class EmscriptenWSClientInterface : public ClientInterface
{
public:
    EmscriptenWSClientInterface(ClientInterfaceCallback& cb, const std::string& url);
    DELETE_COPY_MOVE(EmscriptenWSClientInterface);

    virtual void Send(std::string_view data) override;
    virtual void Poll() override;

    virtual ~EmscriptenWSClientInterface() override;

private:
    int ws_;
};

}

#endif // EMSCRIPTEN