#pragma once

#include <functional>
#include <deque>

#include "game/player_input.hpp"
#include "gfx/renderer.hpp"
#include "gfx/text.hpp"
#include "audio/master.hpp"
#include "net/msg_producer.hpp"
#include "net/inmessage.hpp"

#include "gameview/client_session.hpp"

struct ChatMessage
{
    std::unique_ptr<gfx::Text> text;
    float timeout = 0.0f;
    glm::vec4 color = glm::vec4(1.0f);
};

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

    float GetTime() const { return time_; }
    float GetDeltaTime() const { return delta_time_; }

    audio::Master& GetAudioMaster() { return audiomaster_; }

    void AddChatMessage(const std::string& text);
    void AddChatMessagePrefix(const std::string& prefix, const std::string& text);

    ~App();

private:
    void SendInput(game::PlayerInputType type, bool enable);

    void InitChat();
    void UpdateChat();
    void DrawChat(gfx::DrawList& dlist);

private:
    float time_ = 0.0f;
    glm::ivec2 viewport_size_ = {800, 600};
    game::PlayerInputFlags input_ = 0;
    game::PlayerInputFlags prev_input_ = 0;


    float prev_time_ = 0.0f;
    float delta_time_ = 0.0f;

    gfx::Renderer renderer_;
    gfx::DrawList dlist_;

    audio::Master audiomaster_;

    std::unique_ptr<game::view::ClientSession> session_;

    std::shared_ptr<const gfx::Font> font_;

    std::deque<ChatMessage> chat_;
    std::vector<gfx::HudPosition> chatpos_;
};
