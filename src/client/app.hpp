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
#include "wsclient.hpp"
#include "assets/precache.hpp"
#include "settings.hpp"

struct ChatMessage
{
    std::string text;
    float timeout = 0.0f;
    glm::vec4 color = glm::vec4(1.0f);
};

enum AppState
{
    APP_STATE_INIT,
    APP_STATE_LOADING,
    APP_STATE_IDLE,
    APP_STATE_CONNECT,
    APP_STATE_CONNECTED,
    APP_STATE_DISCONNECTED,
};

class App
{
public:
    App(const std::string& settings_path);

    void Frame();

    void SetTime(float time) { time_ = time; }
    void SetViewportSize(int width, int height) { viewport_size_ = {width, height}; }

    void SetUrl(const std::string& url) { url_ = url; }
    void SetUserName(const std::string& username) { username_ = username; }
    const std::string& GetUserName() const { return username_; }

    void Input(game::PlayerInputType in, bool pressed, bool repeated);
    void MouseMove(const glm::vec2& delta);

    const float& GetTime() const { return time_; }
    float GetDeltaTime() const { return delta_time_; }

    audio::Master& GetAudioMaster() { return audiomaster_; }

    void AddChatMessage(const std::string& text);
    void AddChatMessagePrefix(const std::string& prefix, const std::string& text);

    ~App();

private:
    void Update();
    void Draw();

    void UpdateChat();
    void DrawChat();

    void OpenSettings();
    void UpdateVolume();

    void UpdateSession();
    void UpdateStats();
    void DrawStats();

    void Connect();
    void ProcessWsMessage(std::span<const char> data);

    void UpdateState();
    void EnterState(AppState state);
    AppState CheckStateTransition();
    float GetCurrentStateDuration() const { return time_ - state_time_; }

private:
    Settings settings_;

    float time_ = 0.0f;
    glm::ivec2 viewport_size_ = {800, 600};

    float prev_time_ = 0.0f;
    float delta_time_ = 0.0f;

    gfx::Renderer renderer_;
    gfx::DrawList dlist_;
    gui::Context gui_;
    audio::Master audiomaster_;

    WsClient ws_;
    std::string url_;
    std::string username_;
    bool connecting_ = false;
    bool connected_ = false;
    bool local_error_ = false;

    assets::Precache precache_;

    std::unique_ptr<game::view::ClientSession> session_;

    std::deque<ChatMessage> chat_;
    std::unique_ptr<gui::Menu> menu_;

    AppState state_ = APP_STATE_INIT;
    float state_time_ = 0.0f;

    // settings
    // int volume_ = 20;
    // int sens_ = 50;
    // float sensitivity_ = 0.0f;


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
