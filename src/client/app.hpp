#pragma once

#include <functional>
#include <deque>

#include "game/player_input.hpp"
#include "gfx/renderer.hpp"
#include "gui/font.hpp"
#include "gui/context.hpp"
#include "audio/master.hpp"
#include "net/msg_producer.hpp"
#include "net/inmessage.hpp"
#include "gui/menu.hpp"
#include "gameview/client_session.hpp"

struct ChatMessage
{
    std::string text;
    float timeout = 0.0f;
    glm::vec4 color = glm::vec4(1.0f);
};

class App
{
public:
    App();

    void Frame();

    void Connected();
    void ProcessMessage(net::InMessage& msg);
    void Disconnected(const std::string& reason);

    void SetTime(float time) { time_ = time; }
    void SetViewportSize(int width, int height) { viewport_size_ = {width, height}; }

    void SetUserName(const std::string& username) { username_ = username; }
    const std::string& GetUserName() const { return username_; }

    void Input(game::PlayerInputType in, bool pressed, bool repeated);
    void MouseMove(const glm::vec2& delta);

    const float& GetTime() const { return time_; }
    float GetDeltaTime() const { return delta_time_; }

    game::view::ClientSession* GetSession() { return session_.get(); }

    audio::Master& GetAudioMaster() { return audiomaster_; }

    void AddChatMessage(const std::string& text);
    void AddChatMessagePrefix(const std::string& prefix, const std::string& text);

    ~App();

private:
    void UpdateChat();
    void DrawChat();

    void OpenSettings();
    void ApplySettings();
    void ApplyVolume();
    void ApplySensitivity();

    void UpdateStats();
    void DrawStats();

private:
    float time_ = 0.0f;
    glm::ivec2 viewport_size_ = {800, 600};

    float prev_time_ = 0.0f;
    float delta_time_ = 0.0f;

    gfx::Renderer renderer_;
    gfx::DrawList dlist_;
    gui::Context gui_;

    audio::Master audiomaster_;

    std::string username_;
    std::unique_ptr<game::view::ClientSession> session_;

    std::deque<ChatMessage> chat_;

    std::unique_ptr<gui::Menu> menu_;

    // settings
    int volume_ = 50;
    int sens_ = 50;
    float sensitivity_ = 0.0f;

    // stats
    float stats_time_ = 0.0f;
    size_t stat_frames_ = 0;
    size_t stat_msgs_ = 0;
    size_t stat_msglen_total_ = 0;
    size_t stat_msglen_min_ = SIZE_MAX;
    size_t stat_msglen_max_ = 0;
    std::string fps_text_ = { 0 };
    std::string msglen_text_ = { 0 };
};
