#include "app.hpp"

#include <iostream>

App::App()
{
	std::cout << "Initializing App..." << std::endl;


}

void App::Frame()
{
	float delta_time = time_ - prev_time_;
	prev_time_ = time_;

	if (delta_time < 0.0f)
	{
		delta_time = 0.0f; // Prevent negative delta time
	}
	else if (delta_time > 0.1f)
	{
		delta_time = 0.1f; // Cap delta time to avoid large jumps
	}

	// detect inputs originating in this frame
	game::PlayerInputFlags new_input = input_ & ~prev_input_;
	prev_input_ = input_;

	float aspect = static_cast<float>(viewport_size_.x) / static_cast<float>(viewport_size_.y);

	renderer_.Begin(viewport_size_.x, viewport_size_.y);

	// TODO: draw world
}

void App::MouseMove(const glm::vec2& delta)
{
	float sensitivity = 0.002f; // Sensitivity factor for mouse movement

	float delta_yaw = delta.x * sensitivity;
	float delta_pitch = -delta.y * sensitivity;

	// TODO: rotate
}

App::~App()
{

}
