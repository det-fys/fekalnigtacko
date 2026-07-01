#pragma once

#include "server.hpp"
#include "local_channel_pair.hpp"

namespace net
{

class LocalServerInterface : public ServerInterface
{
public:
    LocalServerInterface(std::shared_ptr<LocalChannelPair> chan);
    DELETE_COPY_MOVE(LocalServerInterface);

    virtual bool PollEvent(ServerInterfaceEvent& ev) override;
    virtual void Send(ConnId conn, std::string_view data) override;
    virtual void CloseConnection(ConnId conn) override;

    virtual ~LocalServerInterface() override;

private:
    std::shared_ptr<LocalChannelPair> chan_;
    bool connected_ = false;
    bool client_connected_ = false;
};


}