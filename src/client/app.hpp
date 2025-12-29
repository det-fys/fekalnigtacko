#pragma once

#include "game/player_input.hpp"
#include "gfx/renderer.hpp"

class App
{
    float time_ = 0.0f;
	glm::ivec2 viewport_size_ = { 800, 600 };
    game::PlayerInputFlags input_ = 0;
    game::PlayerInputFlags prev_input_ = 0;

	float prev_time_ = 0.0f;

    gfx::Renderer renderer_;

public:
    App();

    void Frame();

    void SetTime(float time) { time_ = time; }
	void SetViewportSize(int width, int height) { viewport_size_ = { width, height }; }
	void SetInput(game::PlayerInputFlags input) { input_ = input; }
    void MouseMove(const glm::vec2& delta);

    ~App();
};

