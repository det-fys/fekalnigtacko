#include "app.hpp"

#include <iostream>

#include "net/defs.hpp"
#include "net/outmessage.hpp"
#include "assets/cache.hpp"
#include "gameview/worldview.hpp"

App::App() :
	gui_(dlist_, assets::CacheManager::GetFont("data/comic32.font"))
{
	std::cout << "Initializing App..." << std::endl;

#ifndef EMSCRIPTEN
	audiomaster_.SetMasterVolume(1.0f);
#else
	audiomaster_.SetMasterVolume(2.0f);
#endif

	AddChatMessage("Test!");
}

void App::Frame()
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

	if (session_)
	{
		game::view::UpdateInfo updinfo;
		updinfo.time = time_;
		updinfo.delta_time = delta_time_;
		session_->Update(updinfo);
	}


	renderer_.Begin(viewport_size_.x, viewport_size_.y);
	renderer_.ClearColor(glm::vec3(0.5f, 0.7f, 1.0f));
	renderer_.ClearDepth();

	dlist_.Clear();
	gfx::DrawListParams params;
	params.screen_width = viewport_size_.x;
	params.screen_height = viewport_size_.y;
	
	gui_.Begin();

	// draw session
	if (session_)
	{
        session_->Draw(dlist_, params, gui_);
	}

	// draw stats
	UpdateStats();
	DrawStats();

	// draw chat
	UpdateChat();
	DrawChat();

	// draw menu
	if (menu_)
	{
		auto menu_size = menu_->MeasureSize();
		menu_->Draw(gui_, glm::vec2(viewport_size_) - menu_size - 10.0f);
	}

	gui_.Render();
	renderer_.DrawList(dlist_, params);

	++stat_frames_;
}

void App::Connected()
{
	std::cout << "WS connected" << std::endl;
	AddChatMessagePrefix("WebSocket", "^7f7připojeno");

	// init session
	session_ = std::make_unique<game::view::ClientSession>(*this);
}

void App::ProcessMessage(net::InMessage& msg)
{
	if (!session_)
		return;

	size_t s = msg.End() - msg.Ptr();
 //   AddChatMessage("recvd: ^f00;" + std::to_string(s));

	// std::cout << "App::ProcessMessage: received message of size " << s << " bytes" << std::endl;

	if (!session_->ProcessMessage(msg))
	{
        std::cerr << "FAILED to process message!" << std::endl;
	}

	// record stats
	++stat_msgs_;
	stat_msglen_total_ += s;
	stat_msglen_min_ = std::min(stat_msglen_min_, s);
	stat_msglen_max_ = std::max(stat_msglen_max_, s);
}

void App::Disconnected(const std::string& reason)
{
	std::cout << "WS disconnected" << std::endl;
	AddChatMessagePrefix("WebSocket", "^f77spojení je píči");


	// close session
	session_.reset();
}

static bool InputToMenuInput(game::PlayerInputType& in, gui::MenuInput& mi)
{
	switch (in)
	{
		case game::IN_FORWARD: mi = gui::MI_UP; return true;
		case game::IN_BACKWARD: mi = gui::MI_DOWN; return true;
		case game::IN_LEFT: mi = gui::MI_LEFT; return true;
		case game::IN_RIGHT: mi = gui::MI_RIGHT; return true;
		case game::IN_JUMP: mi = gui::MI_ENTER; return true;
		case game::IN_CROUCH: mi = gui::MI_BACK; return true;
		default: return false;
	}
};

void App::Input(game::PlayerInputType in, bool pressed, bool repeated)
{
	if (in == game::IN_MENU && pressed)
	{
		OpenSettings();
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
	float sensitivity = 0.002f; // Sensitivity factor for mouse movement

	float delta_yaw = -delta.x * sensitivity;
	float delta_pitch = -delta.y * sensitivity;

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

static void AddSlider(gui::Menu& menu, std::string text, int& value, int min, int max)
{
	auto& slider = menu.Add<gui::SelectMenuItem>(std::move(text));
	auto on_switch = [&slider, &value, min, max] (int v) {
		value += v;
		
		// clamp
		if (value < min)
			value = min;
		else if (value > max)
			value = max;

		slider.SetSelectionText(std::to_string(value));
	};

	slider.SetSwitchCallback(on_switch);
	on_switch(0);
}

void App::OpenSettings()
{
	menu_ = std::make_unique<gui::Menu>();

	AddSlider(*menu_, "jak moc to řve", volume_, 0, 100);
	
	auto& ok = menu_->Add<gui::ButtonMenuItem>("0k");
	ok.SetClickCallback([this] { menu_.reset(); });
}

#define COL_LABEL "^ccc"
#define COL_VALUE "^5ff"

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
