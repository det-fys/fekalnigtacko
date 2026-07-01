#pragma once

#include <cstdint>
#include <vector>
#include <deque>
#include <mutex>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <condition_variable>

#include "server.hpp"

namespace crow::websocket
{
    class connection;
}

namespace net
{

class CrowWSServerInterface : public ServerInterface
{
public:
    CrowWSServerInterface(uint16_t port);
    DELETE_COPY_MOVE(CrowWSServerInterface);

    virtual bool PollEvent(ServerInterfaceEvent& ev) override;
    virtual void Send(ConnId conn, std::string_view data) override;
    virtual void CloseConnection(ConnId conn) override;

    void Exit();

    virtual ~CrowWSServerInterface() override;

private:
    void PushEvent(std::unique_lock<std::mutex>& lock, const ServerInterfaceEvent& event);

private:
    std::deque<ServerInterfaceEvent> events_;
    std::mutex mtx_;
    std::condition_variable not_full_;

    std::unique_ptr<std::thread> ws_thread_;
    void* app_ptr_ = nullptr;

    std::unordered_map<ConnId, crow::websocket::connection*> id2conn_;
    ConnId last_id_ = 0;
};

}
