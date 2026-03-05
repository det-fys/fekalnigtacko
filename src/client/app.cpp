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

	// detect inputs originating in this frame
	//game::PlayerInputFlags new_input = input_ & ~prev_input_;

	// detect input changes
	for (size_t i = 0; i < game::IN__COUNT; ++i)
	{
		auto in_old = prev_input_ & (1 << i);
		auto in_new = input_ & (1 << i);

		if (in_old > in_new) // released
		{
			SendInput(static_cast<game::PlayerInputType>(i), false);
		}
		else if (in_new > in_old) // pressed
		{
			SendInput(static_cast<game::PlayerInputType>(i), true);
		}
	}

	prev_input_ = input_;

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

	const game::view::WorldView* world;
	if (session_)
	{
        session_->Draw(dlist_, params, gui_);
	}

	// draw chat
	UpdateChat();
	DrawChat();

	gui_.Render();
	renderer_.DrawList(dlist_, params);
}

void App::Connected()
{
	std::cout << "WS connected" << std::endl;
	AddChatMessagePrefix("WebSocket", "^7f7připojeno");

	// init session
	session_ = std::make_unique<game::view::ClientSession>(*this);

	// send login
	auto msg = BeginMsg(net::MSG_ID);
	net::PlayerName name;
	msg.Write(name);
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
}

void App::Disconnected(const std::string& reason)
{
	std::cout << "WS disconnected" << std::endl;
	AddChatMessagePrefix("WebSocket", "^f77spojení je píči");


	// close session
	session_.reset();
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

void App::SendInput(game::PlayerInputType type, bool enable)
{
	auto msg = BeginMsg(net::MSG_IN);
	uint8_t val = type;
	if (enable)
		val |= 128;
	msg.Write(val);
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
