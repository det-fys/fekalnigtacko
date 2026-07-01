#pragma once

#include "client.hpp"
#include "local_channel_pair.hpp"

namespace net
{

class LocalClientInterface : public ClientInterface
{
public:
    LocalClientInterface(ClientInterfaceCallback& cb, std::shared_ptr<LocalChannelPair> chan);
    DELETE_COPY_MOVE(LocalClientInterface);

    virtual void Send(std::string_view data) override;
    virtual void Poll() override;

    virtual ~LocalClientInterface() override;

private:
    std::shared_ptr<LocalChannelPair> chan_;
    bool connected_ = false;
};

}
