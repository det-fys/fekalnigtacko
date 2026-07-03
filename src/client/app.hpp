#pragma once

#include <functional>
#include <deque>
#include <thread>

#include "game/player_input.hpp"
#include "gfx/renderer.hpp"
#include "gui/font.hpp"
#include "gui/context.hpp"
#include "audio/master.hpp"
#include "net/msg_producer.hpp"
#include "net/inmessage.hpp"
#include "gui/menu.hpp"
#include "gameview/client_session.hpp"
#include "net/client.hpp"
#include "assets/precache.hpp"
#include "settings.hpp"
#include "gui/chat.hpp"
#include "utils/keys.hpp"
#include "utils/cmdlinestream.hpp"

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
    APP_STATE_DISCONNECT,
    APP_STATE_DISCONNECTED,
    APP_STATE_CONNECT_LOCAL,
    APP_STATE_CONNECTED_LOCAL,
    APP_STATE_DISCONNECT_LOCAL,
    APP_STATE_DISCONNECTED_LOCAL,
};

class App : public net::ClientInterfaceCallback
{
public:
    App(const std::string& settings_path);

    virtual void OnClientConnect() override;
    virtual void OnClientMessage(std::string_view data) override;
    virtual void OnClientDisconnect() override;

    void Frame();

    void SetTime(float time) { time_ = time; }
    void SetViewportSize(int width, int height) { viewport_size_ = {width, height}; }

    void SetUrl(const std::string& url) { url_ = url; }
    void SetUserName(const std::string& username) { username_ = username; }
    const std::string& GetUserName() const { return username_; }

    void Input(game::PlayerInputType in, bool pressed, bool repeated);
    void MouseMove(const glm::vec2& delta);

    bool KeyInput(KeyCode key, bool pressed, size_t repeat);
    void TextInput(std::string_view text);

    const float& GetTime() const { return time_; }
    float GetDeltaTime() const { return delta_time_; }
    bool IsFullscreenRequested() const { return fullscreen_; }

    audio::Master& GetAudioMaster() { return audiomaster_; }

    void AddChatMessagePrefix(const std::string& prefix, const std::string& text);
    void AddChatMessage(std::string text);

    ~App();

private:
    void Update();
    void Draw();

    void OpenSettings();
    void UpdateVolume();

    void UpdateSession();
    void UpdateStats();
    void DrawStats();

    void Connect();
    void ConnectLocal();
    void Disconnect();

    void UpdateState();
    void EnterState(AppState state);
    AppState CheckStateTransition();
    float GetCurrentStateDuration() const { return time_ - state_time_; }

    void ProcessChatInput(std::string input);

    void ProcessLocalCommand(std::string_view line);
    void ProcessSetCmd(CmdLineStream& line);
    void ProcessServerCmd(CmdLineStream& line);
    void ProcessConnectOrDisconnectCmd(CmdLineStream& line, bool connect);

private:
    Settings settings_;

    float time_ = 0.0f;
    glm::ivec2 viewport_size_ = {800, 600};
    bool fullscreen_ = false;

    float prev_time_ = 0.0f;
    float delta_time_ = 0.0f;

    gfx::Renderer renderer_;
    gfx::DrawList dlist_;
    gui::Context gui_;
    audio::Master audiomaster_;

    std::jthread server_thread_;
    bool run_local_server_ = false;

    std::unique_ptr<net::ClientInterface> interface_;
    std::string url_;
    std::string username_;
    bool connect_ = false;
    bool connecting_ = false;
    bool connected_ = false;
    bool local_error_ = false;

    assets::Precache precache_;

    gui::Chat chat_;

    std::unique_ptr<game::view::ClientSession> session_;

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
