#include "app.hpp"

#include <iostream>

#include "net/defs.hpp"
#include "net/outmessage.hpp"
#include "assets/cache.hpp"
#include "gameview/worldview.hpp"
#include "gameview/utils.hpp"
#include "gui/loading_screen.hpp"
#include "utils/cvars.hpp"

CVAR(float, sensitivity, CV_SAVE, 0.5f);
CVAR(float, volume, CV_SAVE, 0.2f, 0.0f);

App::App(const std::string& settings_path)
    : settings_(settings_path), gui_(dlist_, assets::CacheManager::GetFont("data/comic32.font")),
      precache_("data/precache")
{
	std::cout << "Initializing App..." << std::endl;

	try
	{
		settings_.Load();
	}
	catch (const std::runtime_error& e)
	{
		AddChatMessagePrefix("Nastavení", "chyba při načítání: " + std::string(e.what()));
	}
}

void App::Frame()
{
	ws_.Poll();

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

void App::AddChatMessage(const std::string& text)
{
	auto& ch = chat_.emplace_back();
	ch.timeout = time_ + 10.0f;
	ch.text = text;
	UpdateChat();
}

void App::AddChatMessagePrefix(const std::string& prefix, const std::string& text)
{
	AddChatMessage("^aaa[^ddd" + prefix + "^aaa]^r " + text);
}

App::~App() {}

void App::Update()
{
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
	UpdateChat();

	settings_.TrySave(time_);
}

void App::Draw()
{

    gfx::DrawListParams params{};
	params.screen_width = viewport_size_.x;
	params.screen_height = viewport_size_.y;
    params.env.clear_color = glm::vec3(0.1f);
	
	dlist_.Clear();
	gui_.Begin(viewport_size_);

	// draw session
	if (session_)
	{
        session_->Draw(dlist_, params, gui_);
	}

	// loading screen
	if (!precache_.IsDone())
	{
		gui::DrawLoadingScreen(gui_, precache_.GetNumLoaded() * 100 / precache_.GetNumItems());
	}

	DrawStats();
	DrawChat();

	// draw menu
	if (menu_)
	{
		auto menu_size = menu_->MeasureSize();
		menu_->Draw(gui_, (glm::vec2(viewport_size_) - menu_size) * 0.5f);
	}

	gui_.Render();
	renderer_.DrawList(dlist_, params);

	++stat_frames_;
}

void App::UpdateChat()
{
	// remove expired or over the limit messages
	while (!chat_.empty() && (chat_.size() > 20 ||chat_[0].timeout < time_))
	{
		chat_.pop_front();
	}
}

void App::DrawChat()
{
	for (size_t i = 0; i < chat_.size(); ++i)
	{
		glm::vec2 pos(10.0f, static_cast<float>(i) * gui_.GetFont()->GetLineHeight() + 10.0f);

		float t_rem = chat_[i].timeout - time_;
        const float fade = 1.0f;
		if (t_rem < fade)
		{
            chat_[i].color.a = t_rem / fade;
		}

		uint32_t color = glm::packUnorm4x8(chat_[i].color);
		gui_.DrawText(chat_[i].text, pos, color);
	}
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
	auto cvar = dynamic_cast<CVar<float>*>(&CVarRegistry::GetCVar(cvar_name));
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

#define COL_LABEL "^ccc"
#define COL_VALUE "^5ff"

void App::UpdateSession()
{
	if (!session_)
		return;

	game::view::UpdateInfo updinfo;
	updinfo.time = time_;
	updinfo.delta_time = delta_time_;
	session_->Update(updinfo);

	if (connected_)
	{
		auto msg = session_->GetMsg();
		if (!msg.empty())
		{
			ws_.Send(msg);
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
	glm::vec2 pos(viewport_size_.x - 5.0f, 5.0f);
	gui_.DrawTextAligned(fps_text_, pos, glm::vec2(-1.0f, 0.0f));
	pos.y += 30.0f;
	gui_.DrawTextAligned(msglen_text_, pos, glm::vec2(-1.0f, 0.0f));
}

void App::Connect()
{
	ws_.SetOnConnect([this]{
		connected_ = true;
		connecting_ = false;
	});

	ws_.SetOnMessage([this](std::span<const char> data) {
		ProcessWsMessage(data);
	});

	ws_.SetOnDisconnect([this]{
		connected_ = false;
		connecting_ = false;
	});

	connecting_ = ws_.Connect(url_);
}

void App::ProcessWsMessage(std::span<const char> data)
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

void App::UpdateState()
{
	auto new_state = CheckStateTransition();
	if (new_state == state_)
		return;

	EnterState(new_state);
}

void App::EnterState(AppState state)
{
	state_ = state;
	state_time_ = time_;

	switch (state)
	{
	case APP_STATE_INIT:
		break;

	case APP_STATE_LOADING:
		break;

	case APP_STATE_IDLE:
		break;

	case APP_STATE_CONNECT:
	    AddChatMessagePrefix("WebSocket", "připojování na " + url_);
		Connect();
		break;

	case APP_STATE_CONNECTED:
		AddChatMessagePrefix("WebSocket", "^7f7připojeno");
		session_ = std::make_unique<game::view::ClientSession>(*this);	
		break;

	case APP_STATE_DISCONNECTED:
		AddChatMessagePrefix("WebSocket", "^f77spojení je píči");
		session_.reset();
		AddChatMessagePrefix("WebSocket", "další pokus za 10 s");
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
		return APP_STATE_LOADING;
	
	case APP_STATE_LOADING:
		if (precache_.IsDone())
			return APP_STATE_IDLE;

		precache_.LoadNext();

		return APP_STATE_LOADING;

	case APP_STATE_IDLE:
		return APP_STATE_CONNECT;

	case APP_STATE_CONNECT:
		if (connected_)
			return APP_STATE_CONNECTED;

		if (!connecting_)
			return APP_STATE_DISCONNECTED;

		return APP_STATE_CONNECT;

	case APP_STATE_CONNECTED:
		if (!connected_)
			return APP_STATE_DISCONNECTED;

		return APP_STATE_CONNECTED;

	case APP_STATE_DISCONNECTED:
		if (GetCurrentStateDuration() >= 10.0f)
			return APP_STATE_CONNECT;

		return APP_STATE_DISCONNECTED;

	default:
		return state_;
	}
}
