#pragma once

#include <cstdint>
#include <vector>
#include <deque>
#include <mutex>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace crow::websocket
{
    class connection;
}

namespace sv
{

enum WSEventType
{
    WSE_NONE,
    WSE_EXIT,
    WSE_CONNECTED,
    WSE_MESSAGE,
    WSE_DISCONNECTED,
};

using WSConnId = uint32_t;

struct WSEvent
{
    WSEventType type = WSE_NONE;
    WSConnId conn = 0;
    std::string data;
};

// using WSEventHandler = std::function<void(const WSEvent&)>;

class WSServer
{
public:
    WSServer(uint16_t port);

    WSServer(const WSServer& other) = delete;
    WSServer(WSServer&& other) = delete;
    WSServer& operator=(const WSServer& other) = delete;
    WSServer& operator=(WSServer&& other) = delete;

    bool PollEvent(WSEvent& out_event);
    void Send(WSConnId conn_id, std::string data);
    // void Close(WSConnId conn_id);

    void Exit();

    ~WSServer();

private:
    std::deque<WSEvent> events_;
    std::mutex mtx_;

    std::unique_ptr<std::thread> ws_thread_;
    void* app_ptr_ = nullptr;

    std::unordered_map<WSConnId, crow::websocket::connection*> id2conn_;
    WSConnId last_id_ = 0;

};

}