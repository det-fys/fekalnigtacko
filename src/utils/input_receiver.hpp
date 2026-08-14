#pragma once

#include <string_view>

#include <glm/glm.hpp>

#include "utils/keys.hpp"

class InputReceiver
{
public:
    virtual void KeyInput(KeyCode key, bool pressed, int repeat) = 0;
    virtual void MouseMove(const glm::vec2& delta) = 0;
    virtual void TextInput(std::string_view text) = 0;

    virtual ~InputReceiver() = default;
};
