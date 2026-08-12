#include "app.hpp"

#include <iostream>
#include <map>

#include "net/defs.hpp"
#include "net/outmessage.hpp"
#include "assets/asset_manager.hpp"
#include "gameview/worldview.hpp"
#include "gameview/utils.hpp"
#include "gui/loading_screen.hpp"
#include "utils/cvars.hpp"
#include "utils/keys.hpp"
#include "utils/chatcolors.hpp"
#include "net/client_local.hpp"
#include "net/server_local.hpp"
#include "server/server.hpp"
#include "server/server_cfg.hpp"
#include "db/db_memory.hpp"
#include "version.hpp"

#include "net/client_ws.hpp"

#include <imgui.h>

CVAR_CL(float, sensitivity, CV_SAVE, 0.5f);
CVAR_CL(float, volume, CV_SAVE, 0.2f, 0.0f);

CVAR_CL(uint8_t, app_autoconnect, CV_SAVE, 1, 0, 1);
CVAR_CL(uint8_t, app_autostartserver, CV_SAVE, 0, 0, 1);

CVAR_CL(uint8_t, app_showfps, CV_NONE, 1, 0, 1);
CVAR_CL(uint8_t, app_shownetstats, CV_NONE, 1, 0, 1);
CVAR_CL(uint8_t, app_showviewportsize, CV_SAVE, 0, 0, 1);

static const std::map<KeyCode, game::PlayerInputType> s_inputmap = {
	{ KEY_LMB, game::IN_ATTACK_PRIMARY },
	{ KEY_RMB, game::IN_ATTACK_SECONDARY },
    { KEY_W, game::IN_FORWARD },
    { KEY_S, game::IN_BACKWARD },
    { KEY_A, game::IN_LEFT },
    { KEY_D, game::IN_RIGHT },
    { KEY_SPACE, game::IN_JUMP },
    { KEY_LSHIFT, game::IN_SPRINT },
    { KEY_LCTRL, game::IN_CROUCH },
    { KEY_E, game::IN_USE },
    { KEY_Q, game::IN_HOLSTER },
    { KEY_R, game::IN_RELOAD },
    { KEY_LALT, game::IN_AIM_MODE },
    { KEY_1, game::IN_WEAPON_1 },
    { KEY_2, game::IN_WEAPON_2 },
    { KEY_3, game::IN_WEAPON_3 },
    { KEY_4, game::IN_WEAPON_4 },
    { KEY_5, game::IN_WEAPON_5 },
    { KEY_6, game::IN_WEAPON_6 },
    { KEY_7, game::IN_WEAPON_7 },
    { KEY_8, game::IN_WEAPON_8 },
    { KEY_9, game::IN_WEAPON_9 },
    { KEY_0, game::IN_WEAPON_0 },
    { KEY_F3, game::IN_DEBUG1 },
    { KEY_F4, game::IN_DEBUG2 },
    { KEY_F5, game::IN_DEBUG3 },
    { KEY_F2, game::IN_MENU },
};

App::App(const std::string& settings_path)
    : settings_(settings_path), gui_(assets::AssetManager::GetInstance().Get<gui::Font>("comic32")),
      chat_(gui_, time_)
{
	std::cout << "Initializing App..." << std::endl;
    AddChatMessage("fekální gtačko " FEKAL_VERSION);


	chat_.SetOnInput([this](std::string msg) {
		ProcessChatInput(std::move(msg));
	});

	try
	{
		settings_.Load();
	}
	catch (const std::runtime_error& e)
	{
		AddChatMessagePrefix("Nastavení", "chyba při načítání: " + std::string(e.what()));
	}

	run_local_server_ = app_autostartserver.Get() > 0;
    if (run_local_server_)
    {
        AddChatMessage("poznámka: app_autostartserver = 1");
    }
}

void App::OnClientConnect()
{
    connected_ = true;
    connecting_ = false;

	session_ = std::make_unique<game::view::ClientSession>(*this);	
}

void App::OnClientMessage(std::string_view data)
{
	if (!session_)
		return;

	// record stats
	size_t s = data.size();
	++stat_msgs_;
	stat_msglen_total_ += s;
	stat_msglen_min_ = std::min(stat_msglen_min_, s);
	stat_msglen_max_ = std::max(stat_msglen_max_, s);

	net::InMessage msg(data.data(), data.size());
	if (!session_->ProcessMessage(msg))
	{
        std::cerr << "FAILED to process message!" << std::endl;
		local_error_ = true;
	}
}

void App::OnClientDisconnect()
{
	Disconnect();
}

void App::Draw(const gfx::DrawContext& ctx)
{
    gui_.Begin(ctx.viewport_size);

	// draw session
    if (session_)
    {
        session_->Draw(ctx, gui_);
    }

	if (ctx.pass != gfx::DRAW_PASS_MAIN)
        return;

    // loading screen
	if (assets_fetch_ && assets_fetch_->GetState() == fs::ArchiveFetchState::InProgress)
	{
		auto total_size = assets_fetch_->GetTotalSize();
		auto downloaded_size = assets_fetch_->GetDownloadedSize();

        std::string message = "získávání " + assets_fetch_->GetName() + ": " + std::to_string(downloaded_size) + " / " +
                              std::to_string(total_size) + " bajtů";

        int percent = 0;
		if (total_size > 0)
		{
            percent = static_cast<int>(downloaded_size * 100 / total_size);
		}

        gui::DrawLoadingScreen(gui_, percent, message);
	}
    else if (precache_ && !precache_->IsDone())
    {
		int percent = 0;
		if (precache_->GetNumItems() > 0)
		{
			percent = precache_->GetNumLoaded() * 100 / precache_->GetNumItems();
		}

        gui::DrawLoadingScreen(gui_, percent, "načítání...");
    }

    DrawStats();
    chat_.Draw();

    // draw menu
    if (menu_)
    {
        auto menu_size = menu_->MeasureSize();
        menu_->Draw(gui_, (glm::vec2(ctx.viewport_size) - menu_size) * 0.5f);
    }

    gui_.Render(ctx.dlist);
}

gfx::Environment App::GetSceneEnvironment()
{
    return session_ ? session_->GetEnv() : gfx::Environment{};
}

float App::GetMapChunkSize()
{
    return session_ ? session_->GetMapChunkSize() : 1.0f;
}

void App::Frame()
{
	Update();
	Draw();
}

void App::Input(game::PlayerInputType in, bool pressed, bool repeated)
{
	if (in == game::IN_MENU && pressed)
	{
		if (!menu_)
			OpenSettings();
		else
			menu_.reset();

		return;
	}

	gui::MenuInput mi;
	if (menu_ && pressed && InputToMenuInput(in, mi))
	{
		menu_->Input(mi);
		return;
	}

	if (session_)
		session_->Input(in, pressed, repeated);

}

void App::MouseMove(const glm::vec2& delta)
{
	auto sens = glm::mix(0.0005f, 0.0035f, sensitivity.Get());

	float delta_yaw = -delta.x * sens;
	float delta_pitch = -delta.y * sens;

	if (session_)
		session_->ProcessMouseMove(delta_yaw, delta_pitch);
}

bool App::KeyInput(KeyCode key, bool pressed, size_t repeat)
{
	if (pressed)
	{
		if (chat_.KeyInput(key))
			return true;

		if (key == KEY_F)
		{
			fullscreen_ = !fullscreen_;
			return true;
		}

		// TODO: menu controls here
	}

	auto it = s_inputmap.find(key);
	if (it != s_inputmap.end())
	{
		Input(it->second, pressed, repeat > 0);
	}

	return true;

}

void App::TextInput(std::string_view text)
{
	if (!chat_.IsWindowOpen())
		return;

	chat_.TextInput(text);
}

void App::AddChatMessagePrefix(const std::string& prefix, const std::string& text)
{
	AddChatMessage("^aaa[^ddd" + prefix + "^aaa]^r " + text);
}

void App::AddChatMessage(std::string text)
{
	chat_.AddMessage(std::move(text));
}

App::~App() {}

void App::Update()
{
    if (interface_)
        interface_->Poll();

	delta_time_ = time_ - prev_time_;
	prev_time_ = time_;

	if (delta_time_ < 0.0f)
	{
		delta_time_ = 0.0f; // Prevent negative delta time
	}
	else if (delta_time_ > 0.1f)
	{
		delta_time_ = 0.1f; // Cap delta time to avoid large jumps
	}

	UpdateVolume();
	UpdateState();
	UpdateSession();
	UpdateStats();
	chat_.Update();
	
	settings_.TrySave(time_);

	ImGui::ShowDemoWindow();
}

void App::Draw()
{
    gfx::CameraParams cam{};
	if (session_)
	{
        cam = session_->GetCameraParams();
	}

	gfx::Renderer::GetInstance().Draw(*this, cam);
	++stat_frames_;
}

static void AddSlider(gui::Menu& menu, std::string text, int& value, int min, int max, std::function<void()> changed)
{
	auto& slider = menu.Add<gui::SelectMenuItem>(std::move(text));
	auto on_switch = [&slider, &value, min, max, changed] (int v) {
		value += v;
		
		// clamp
		if (value < min)
			value = min;
		else if (value > max)
			value = max;

		slider.SetSelectionText(std::to_string(value) + " %");
		changed();
	};

	slider.SetSwitchCallback(on_switch);
	on_switch(0);
}

static void CreatePercentSlider(gui::Menu& menu, std::string text, const std::string& cvar_name, float max)
{
    auto cvar = dynamic_cast<CVar<float>*>(&CVarRegistry::GetClientInstance().GetCVar(cvar_name));
	if (!cvar)
		return; // not float cvar

	float current_value = cvar->Get();
	int percent = glm::clamp(static_cast<int>(glm::round(current_value * 100.0f / max)), 0, 100);

	auto& slider = menu.Add<gui::SelectMenuItem>(std::move(text));
	slider.SetSelectionText(std::to_string(percent) + " %");

	slider.SetSwitchCallback([&slider, cvar, max, percent](int v) mutable {
		if (v == 0 || (percent <= 0 && v < 0) || (percent >= 100 && v > 0))
			return;

		percent += v;
		cvar->Set(static_cast<float>(percent) * 0.01f * max);
		slider.SetSelectionText(std::to_string(percent) + " %");
	});
}

void App::OpenSettings()
{
	menu_ = std::make_unique<gui::Menu>();
	menu_->SetTitle("nastavení");

	CreatePercentSlider(*menu_, "jak moc to řve", "volume", 1.0f);
	CreatePercentSlider(*menu_, "agresivita krysy", "sensitivity", 1.0f);

	auto& ok = menu_->Add<gui::ButtonMenuItem>("0k");
	ok.SetClickCallback([this] { menu_.reset(); });
}

void App::UpdateVolume()
{
	if (!volume.IsModified())
		return;

	
	audiomaster_.SetMasterVolume(4.0f * volume.Get());
	volume.ClearModified();
}

void App::UpdateSession()
{
	if (!session_)
		return;

	game::view::UpdateInfo updinfo;
	updinfo.time = time_;
	updinfo.delta_time = delta_time_;
	session_->Update(updinfo);

	if (connected_ && interface_)
	{
		auto msg = session_->GetMsg();
		if (!msg.empty())
		{
			std::string_view data(msg.data(), msg.size_bytes());
			interface_->Send(data);
		}

		session_->ResetMsg();
	}
}

void App::UpdateStats()
{
	if (time_ < stats_time_ + 1.0f)
		return;

	stats_time_ = time_;

	fps_text_.clear();
	fps_text_ += COL_VALUE;
	fps_text_ += std::to_string(stat_frames_);
	fps_text_ += COL_LABEL " fps";

	if (stat_msgs_ > 0)
	{
		msglen_text_ = COL_LABEL "net: n=" COL_VALUE;
		msglen_text_ += std::to_string(stat_msgs_);
		msglen_text_ += COL_LABEL " min=" COL_VALUE;
		msglen_text_ += std::to_string(stat_msglen_min_);
		msglen_text_ += COL_LABEL " max=" COL_VALUE;
		msglen_text_ += std::to_string(stat_msglen_max_);
		msglen_text_ += COL_LABEL " total=" COL_VALUE;
		msglen_text_ += std::to_string(stat_msglen_total_);
	}

    stat_frames_ = 0;
    stat_msgs_ = 0;
    stat_msglen_total_ = 0;
    stat_msglen_min_ = SIZE_MAX;
    stat_msglen_max_ = 0;
}

void App::DrawStats()
{
	auto& viewport_size = gui_.GetViewportSize();
	glm::vec2 pos(viewport_size.x - 5.0f, 5.0f);
	if (app_showfps.Get() > 0)
	{
		gui_.DrawTextAligned(fps_text_, pos, glm::vec2(-1.0f, 0.0f), 0xFFFFFFFF, 0.8f);
		pos.x -= 100.0f;
	}

	if (app_shownetstats.Get() > 0)
	{
		gui_.DrawTextAligned(msglen_text_, pos, glm::vec2(-1.0f, 0.0f), 0xFFFFFFFF, 0.8f);
	}
	
	if (app_showviewportsize.Get() > 0)
	{
        auto viewport_size = glm::ivec2(gui_.GetRealViewportSize());
		pos.y += 30.0f;
		std::string text = "^f70" + std::to_string(viewport_size.x) + "x" + std::to_string(viewport_size.y);
		gui_.DrawTextAligned(text, pos, glm::vec2(-1.0f, 0.0f));
	}
}

void App::Connect()
{
	connecting_ = true;
	connected_ = false;

	interface_ = std::make_unique<net::WSClientInterface>(*this, url_);
}

void App::ConnectLocal()
{
	auto channel_pair = std::make_shared<net::LocalChannelPair>();

    server_thread_ = std::jthread([channel_pair] {
		std::cout << "Launching local server..." << std::endl;

		try 
		{
            sv::LoadCfg("server_local.cfg");
		
			// setup game
			game::GameInfo g_info{};
            g_info.db = std::make_unique<db::MemoryGameDatabase>();
            auto game = std::make_unique<game::Game>(std::move(g_info));

			// setup server
			sv::ServerInfo sv_info{};
            sv_info.iface = std::make_unique<net::LocalServerInterface>(channel_pair);
            sv_info.game = std::move(game);
			sv::Server server(std::move(sv_info));

			// run server
			server.Run();
		}
		catch (const std::exception& e)
		{
            std::cout << "LOCAL SERVER ERROR: " << e.what() << std::endl;
        }
	});

	connecting_ = true;
	connected_ = false;

    interface_ = std::make_unique<net::LocalClientInterface>(*this, channel_pair);
}

void App::Disconnect() 
{
	connected_ = false;
    connecting_ = false;
	interface_.reset();
	session_.reset();

	if (server_thread_.joinable())
	{
		server_thread_.join();
	}
}

void App::UpdateState()
{
	auto new_state = CheckStateTransition();
	if (new_state == state_)
		return;

	EnterState(new_state);
}

void App::EnterState(AppState state)
{
	std::cout << "Entering app state " << std::to_string(state) << std::endl;

	state_ = state;
	state_time_ = time_;

	switch (state)
	{
	case APP_STATE_INIT:
		break;

	case APP_STATE_FETCHING_ASSETS:
        assets_fetch_.emplace("game.tsrp");
		break;

	case APP_STATE_LOADING:
        precache_.emplace("data/precache");
		break;

	case APP_STATE_IDLE:
        if (app_autoconnect.Get() > 0 && !run_local_server_)
        {
            connect_ = true;
        }
		break;

	case APP_STATE_CONNECT:
	    AddChatMessagePrefix("WebSocket", "připojování na " + url_);
		Connect();
		break;

	case APP_STATE_CONNECTED:
		AddChatMessagePrefix("WebSocket", COL_SUCCESS "připojeno");
		break;

	case APP_STATE_DISCONNECT:
		Disconnect();
		AddChatMessagePrefix("WebSocket", "vodpojeno");
		break;

	case APP_STATE_DISCONNECTED:
		Disconnect();
		AddChatMessagePrefix("WebSocket", COL_ERROR "spojení je píči");
		//AddChatMessagePrefix("WebSocket", "další pokus za 10 s");
        connect_ = false;
		break;

	case APP_STATE_CONNECT_LOCAL:
		connect_ = false;
		AddChatMessage("připojování na lokální servr");
		ConnectLocal();
		break;

	case APP_STATE_CONNECTED_LOCAL:
		AddChatMessage(COL_SUCCESS "připojeno");
		session_ = std::make_unique<game::view::ClientSession>(*this);	
		break;

	case APP_STATE_DISCONNECT_LOCAL:
		Disconnect();
		run_local_server_ = false;
		AddChatMessage("vodpojeno");
		break;

	case APP_STATE_DISCONNECTED_LOCAL:
		Disconnect();
		run_local_server_ = false;
		break;

	case APP_STATE_ERROR:
		break;

	default:
		break;
	}
}

AppState App::CheckStateTransition()
{
	switch (state_)
	{
	case APP_STATE_INIT:
        return APP_STATE_FETCHING_ASSETS;
	
	case APP_STATE_FETCHING_ASSETS:
		// failure
		if (assets_fetch_->GetState() == fs::ArchiveFetchState::Failed)
		{
			std::cerr << "Failed to fetch assets: " << assets_fetch_->GetError() << std::endl;
            AddChatMessage(COL_ERROR "chyba při získávání assetů: " + assets_fetch_->GetError());
			assets_fetch_.reset();
			return APP_STATE_ERROR;
		}

		// completed
		if (assets_fetch_->GetState() == fs::ArchiveFetchState::Completed)
		{
			std::cout << "Assets fetched successfully, loading..." << std::endl;
			assets_fetch_.reset();
			return APP_STATE_LOADING;
		}

		return APP_STATE_FETCHING_ASSETS;

	case APP_STATE_LOADING:
		if (precache_->IsDone())
		{
			return APP_STATE_IDLE;
		}

		precache_->LoadNext();

		return APP_STATE_LOADING;

	case APP_STATE_IDLE:
        if (run_local_server_)
            return APP_STATE_CONNECT_LOCAL;

		if (connect_)
            return APP_STATE_CONNECT;

		return APP_STATE_IDLE;

	case APP_STATE_CONNECT:
		if (run_local_server_ || !connect_)
			return APP_STATE_DISCONNECT;

		if (connected_)
			return APP_STATE_CONNECTED;

		if (!connecting_)
			return APP_STATE_DISCONNECTED;

		return APP_STATE_CONNECT;

	case APP_STATE_CONNECTED:
		if (run_local_server_ || !connect_)
			return APP_STATE_DISCONNECT;

		if (!connected_)
			return APP_STATE_DISCONNECTED;

		return APP_STATE_CONNECTED;

	case APP_STATE_DISCONNECT:
		return APP_STATE_IDLE;

	case APP_STATE_DISCONNECTED:
		if (run_local_server_ || connect_)
			return APP_STATE_IDLE;

		if (GetCurrentStateDuration() >= 10.0f)
			return APP_STATE_IDLE;

		return APP_STATE_DISCONNECTED;

	case APP_STATE_CONNECT_LOCAL:
		if (!run_local_server_ || connect_)
			return APP_STATE_DISCONNECT_LOCAL;

		if (connected_)
			return APP_STATE_CONNECTED_LOCAL;

		if (!connecting_)
			return APP_STATE_DISCONNECTED_LOCAL;

		return APP_STATE_CONNECT_LOCAL;

	case APP_STATE_CONNECTED_LOCAL:
		if (!run_local_server_ || connect_)
			return APP_STATE_DISCONNECT_LOCAL;

		if (!connected_)
			return APP_STATE_DISCONNECTED_LOCAL;

		return APP_STATE_CONNECTED_LOCAL;

	case APP_STATE_DISCONNECT_LOCAL:
		return APP_STATE_IDLE;

	case APP_STATE_DISCONNECTED_LOCAL:
		if (GetCurrentStateDuration() >= 1.0f)
			return APP_STATE_IDLE;

		return APP_STATE_DISCONNECTED_LOCAL;

	case APP_STATE_ERROR:
		return APP_STATE_ERROR;

	default:
		return state_;
	}
}

void App::ProcessChatInput(std::string input)
{
	std::string_view line = input;
	
	if (line.empty())
		return;

	if (line[0] == '\\')
	{
		// local command
		line.remove_prefix(1);		
		ProcessLocalCommand(line);
		return;
	}

	if (!session_)
	{
		chat_.AddMessage(COL_ERROR "nejsi připojen");
		return;
	}

	session_->ChatInput(line);
}

void App::ProcessLocalCommand(std::string_view line)
{
	CmdLineStream iss(line);

	if (iss.Eol())
		return;

	try
	{
		std::string cmd;
		iss >> cmd;

		if (cmd.empty())
			return;

		if (cmd == "set")
		{
			ProcessSetCmd(iss);
		}
		else if (cmd == "server")
		{
			ProcessServerCmd(iss);
		}
		else if (cmd == "connect")
		{
            ProcessConnectOrDisconnectCmd(iss, true);
		}
		else if (cmd == "disconnect")
		{
            ProcessConnectOrDisconnectCmd(iss, false);
		}
		else
		{
			throw std::runtime_error("neznámej příkaz: " + cmd);
		}
    }
    catch (const std::exception& e)
    {
		chat_.AddMessage(COL_ERROR "chyba: " + std::string(e.what()));
	}
}

void App::ProcessSetCmd(CmdLineStream& line)
{
    auto& registry = CVarRegistry::GetClientInstance();

	if (line.Eol())
	{
		// no args - list cvars

        registry.ProcessCVars([this](CVarBase& cvar) {
            chat_.AddMessage(COL_LABEL + cvar.GetName() + "^r=" COL_VALUE + cvar.GetString());
            return false;
        });
	
		return;
	}

	std::string cvar_name;
	line >> cvar_name;

	auto& cvar = registry.GetCVar(cvar_name);

	if (line.Eol())
	{
		// no value - only print current
        chat_.AddMessage(COL_LABEL + cvar_name + "^r=" COL_VALUE + cvar.GetString());
        return;
	}

	std::string val;
	line >> val;

	cvar.SetString(val);

	chat_.AddMessage(COL_LABEL + cvar_name + "^r nastaveno na " COL_VALUE + cvar.GetString());
}

void App::ProcessServerCmd(CmdLineStream& line)
{
	std::string server_cmd;
	line >> server_cmd;

	if (server_cmd == "start")
	{
		run_local_server_ = true;
	}
	else if (server_cmd == "stop")
	{
		run_local_server_ = false;
	}
	else
	{
		throw std::runtime_error("start nebo stop");
	}

	AddChatMessage(COL_SUCCESS "ok");
}

void App::ProcessConnectOrDisconnectCmd(CmdLineStream& line, bool connect)
{
	if (connect == connect_)
	{
        AddChatMessage(COL_ERROR "tak už to je");
        return;
	}

	connect_ = connect;
    AddChatMessage(COL_SUCCESS "ok");
}
