#include "server_ws_crow.hpp"

#include <crow.h>

#include "utils/allocnum.hpp"

static constexpr size_t MAX_QUEUE_SIZE = 64;

net::CrowWSServerInterface::CrowWSServerInterface(uint16_t port)
{
    ws_thread_ = std::make_unique<std::thread>([this, port]() {
        crow::SimpleApp app;
        {
            std::lock_guard<std::mutex> lock(mtx_);

            app_ptr_ = (void*)&app;

            CROW_WEBSOCKET_ROUTE(app, "/ws")
                .onopen([&](crow::websocket::connection& conn) {
                    CROW_LOG_INFO << "new websocket connection from " << conn.get_remote_ip();

                    std::unique_lock<std::mutex> lock(mtx_);
                    conn.set_nodelay();

                    ConnId conn_id = utils::AllocNum(id2conn_, last_id_);

                    // register connection
                    id2conn_[conn_id] = &conn;
                    conn.userdata(reinterpret_cast<void*>(static_cast<uintptr_t>(conn_id)));
                    // push connection event
                    PushEvent(lock, {SVE_CONNECTED, conn_id});
                })
                .onclose([&](crow::websocket::connection& conn, const std::string& reason, uint16_t) {
                    CROW_LOG_INFO << "websocket connection closed: " << reason;

                    std::unique_lock<std::mutex> lock(mtx_);

                    ConnId conn_id = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(conn.userdata()));

                    // push disonnected event
                    PushEvent(lock, {SVE_DISCONNECTED, conn_id});

                    id2conn_.erase(conn_id);
                })
                .onmessage([&](crow::websocket::connection& conn, const std::string& data, bool is_binary) {
                    if (!is_binary)
                        return; // only accept binary messages here

                    std::unique_lock<std::mutex> lock(mtx_);

                    ConnId conn_id = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(conn.userdata()));
                    PushEvent(lock, {SVE_MESSAGE, conn_id, data});
                });

            // CROW_ROUTE(app, "/")
            // ([] {
            //     crow::mustache::context x;
            //     x["servername"] = "127.0.0.1";
            //     auto page = crow::mustache::load("ws.html");
            //     return page.render(x);
            // });
        }

        app.port(port).run();

        std::lock_guard<std::mutex> lock(mtx_);

        // push exit event
        events_.emplace_back(SVE_EXIT);

        app_ptr_ = nullptr;
    });
}

bool net::CrowWSServerInterface::PollEvent(ServerInterfaceEvent& ev)
{
    std::lock_guard<std::mutex> lock(mtx_);

    if (events_.empty())
        return false;

    ev = std::move(events_.front());
    events_.pop_front();
    not_full_.notify_one();
    return true;
}

void net::CrowWSServerInterface::Send(ConnId conn, std::string_view data)
{
    std::lock_guard<std::mutex> lock(mtx_);
    
    auto it = id2conn_.find(conn);
    if (it == id2conn_.end())
    {
        std::cerr << "attempted to send message to unknown conn ID " << conn << std::endl;
        return;
    }
    
    (*it->second).send_binary(std::string(data));
}

void net::CrowWSServerInterface::CloseConnection(ConnId conn)
{
    std::lock_guard<std::mutex> lock(mtx_);

    auto it = id2conn_.find(conn);
    if (it == id2conn_.end())
    {
        std::cerr << "attempted to close unknown conn ID " << conn << std::endl;
        return;
    }
    
    (*it->second).close();
}

void net::CrowWSServerInterface::Exit()
{
    std::lock_guard<std::mutex> lock(mtx_);
    if (app_ptr_)
        ((crow::SimpleApp*)app_ptr_)->stop();
}

net::CrowWSServerInterface::~CrowWSServerInterface()
{
    Exit();
    if (ws_thread_ && ws_thread_->joinable())
        ws_thread_->join();
}

void net::CrowWSServerInterface::PushEvent(std::unique_lock<std::mutex>& lock, const ServerInterfaceEvent& event)
{
    not_full_.wait(lock, [this] { return events_.size() < MAX_QUEUE_SIZE; });
    events_.emplace_back(event);
}
