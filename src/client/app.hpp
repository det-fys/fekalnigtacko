#pragma once

#include <functional>

#include "game/player_input.hpp"
#include "gfx/renderer.hpp"
#include "net/msg_producer.hpp"
#include "net/inmessage.hpp"

#include "gameview/client_session.hpp"

class App : public net::MsgProducer
{

public:
    App();

    void Frame();

    void Connected();
    void ProcessMessage(net::InMessage& msg);
    void Disconnected(const std::string& reason);

    void SetTime(float time) { time_ = time; }
    void SetViewportSize(int width, int height) { viewport_size_ = {width, height}; }
    void SetInput(game::PlayerInputFlags input) { input_ = input; }
    void MouseMove(const glm::vec2& delta);

    ~App();

private:
    void Send(std::vector<char> data);

private:
    float time_ = 0.0f;
    float last_send_time_ = 0.0f;
    glm::ivec2 viewport_size_ = {800, 600};
    game::PlayerInputFlags input_ = 0;
    game::PlayerInputFlags prev_input_ = 0;

    float prev_time_ = 0.0f;

    gfx::Renderer renderer_;
    gfx::DrawList dlist_;

    std::unique_ptr<game::view::ClientSession> session_;
};
