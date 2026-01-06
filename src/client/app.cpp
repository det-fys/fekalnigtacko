#include "app.hpp"

#include <iostream>

#include "net/defs.hpp"
#include "net/outmessage.hpp"

#include "gameview/worldview.hpp"

App::App()
{
	std::cout << "Initializing App..." << std::endl;


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
	game::PlayerInputFlags new_input = input_ & ~prev_input_;
	prev_input_ = input_;

	if (session_)
	{
		game::view::UpdateInfo updinfo;
		updinfo.time = time_;
		updinfo.delta_time = delta_time_;
		session_->Update(updinfo);
	}

	float aspect = static_cast<float>(viewport_size_.x) / static_cast<float>(viewport_size_.y);

	renderer_.Begin(viewport_size_.x, viewport_size_.y);
	renderer_.ClearColor(glm::vec3(0.3f, 0.9f, 1.0f));
	renderer_.ClearDepth();

	dlist_.Clear();

	const game::view::WorldView* world;
	if (session_ && (world = session_->GetWorld()))
	{
		world->Draw(dlist_);
	
		// glm::mat4 view = glm::lookAt(glm::vec3(15.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, -13.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 3000.0f);
	
		gfx::DrawListParams params;
		params.view_proj = proj * session_->GetViewMatrix();
	
		renderer_.DrawList(dlist_, params);
	}

	if (time_ - last_send_time_ > 0.040f)
	{
        auto msg = BeginMsg(net::MSG_IN);
        msg.Write(input_);

		last_send_time_ = time_;

	}
}

void App::Connected()
{
	std::cout << "WS connected" << std::endl;

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

	session_->ProcessMessage(msg);
}

void App::Disconnected(const std::string& reason)
{
	std::cout << "WS disconnected" << std::endl;

	// close session
	session_.reset();
}

void App::MouseMove(const glm::vec2& delta)
{
	float sensitivity = 0.002f; // Sensitivity factor for mouse movement

	float delta_yaw = delta.x * sensitivity;
	float delta_pitch = -delta.y * sensitivity;

	if (session_)
		session_->ProcessMouseMove(delta_yaw, delta_pitch);
}

App::~App()
{

}
