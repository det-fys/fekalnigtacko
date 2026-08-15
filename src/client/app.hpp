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
#include "gfx/scene.hpp"
#include "gfx/viewport.hpp"
#include "fs/archive_fetch.hpp"
#include "utils/input_receiver.hpp"
#include "im/viewport.hpp"
#include "im/scene_view.hpp"
#include "edit/map_edit.hpp"

struct ChatMessage
{
    std::string text;
    float timeout = 0.0f;
    glm::vec4 color = glm::vec4(1.0f);
};

enum AppState
{
    APP_STATE_INIT,
    APP_STATE_FETCHING_ASSETS,
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
    APP_STATE_ERROR,
};

class App;

class AppViewport : public im::Viewport
{
public:
    using Super = im::Viewport;
    
    AppViewport(App& app);

protected:
    virtual void Update() override;
    virtual void Draw(ImDrawList& draw_list) override;

private:
    App& app_;
    im::SceneView scene_view_;
};

struct DevMode
{
    AppViewport app_viewport;
    bool show_app_viewport = true;

    std::optional<edit::MapEdit> map_edit;
    bool show_map_edit = false;

    bool show_imgui_demo = false;

    DevMode(App& app) : app_viewport(app) {}
};

class App : public net::ClientInterfaceCallback, public gfx::Scene, public InputReceiver
{
public:
    App(const std::string& settings_path);

    // ClientInterfaceCallback
    virtual void OnClientConnect() override;
    virtual void OnClientMessage(std::string_view data) override;
    virtual void OnClientDisconnect() override;

    // Scene
    virtual void Draw(const gfx::DrawContext& ctx) override;
    virtual gfx::Environment GetSceneEnvironment() override;
    virtual float GetMapChunkSize() override;

    // InputReceiver
    virtual void KeyInput(KeyCode key, bool pressed, int repeat) override;
    virtual void MouseMove(const glm::vec2& delta) override;
    virtual void TextInput(std::string_view text) override;

    void Frame();

    void SetTime(float time) { time_ = time; }

    void SetUrl(const std::string& url) { url_ = url; }
    void SetUserName(const std::string& username) { username_ = username; }
    const std::string& GetUserName() const { return username_; }

    void Input(game::PlayerInputType in, bool pressed, bool repeated);

    const float& GetTime() const { return time_; }
    float GetDeltaTime() const { return delta_time_; }
    bool IsFullscreenRequested() const { return fullscreen_; }

    audio::Master& GetAudioMaster() { return audiomaster_; }

    void AddChatMessagePrefix(const std::string& prefix, const std::string& text);
    void AddChatMessage(std::string text);

    gfx::CameraParams GetCameraParams() const;

    ~App();

private:
    void Update();
    void Draw();
    void SwitchDevMode();

    void ShowDevMode();

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
    bool fullscreen_ = false;

    float prev_time_ = 0.0f;
    float delta_time_ = 0.0f;

    gui::Context gui_;
    audio::Master audiomaster_;

    std::optional<fs::ArchiveFetch> assets_fetch_;

    std::jthread server_thread_;
    bool run_local_server_ = false;

    std::unique_ptr<net::ClientInterface> interface_;
    std::string url_;
    std::string username_;
    bool connect_ = false;
    bool connecting_ = false;
    bool connected_ = false;
    bool local_error_ = false;

    std::optional<assets::Precache> precache_;

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

    // dev mode
    bool enable_dev_mode_ = false;
    bool dev_mode_first_frame_ = false;
    std::optional<DevMode> dev_mode_;
};
