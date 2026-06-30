#pragma once

#include <functional>

#include "context.hpp"
#include "utils/keys.hpp"

namespace gui
{

struct ChatMessage
{
    float time = 0.0f;
    std::string text;
};

struct InputChar
{
    uint32_t cp;
    float x;
    float width;
};

using ChatInputCallback = std::function<void(std::string)>; 

class Chat
{
public:
    Chat(Context& ctx, const float& time);

    void AddMessage(std::string message);

    void SetWindowOpen(bool open);

    bool KeyInput(KeyCode key);
    void TextInput(std::string_view text);

    void SetOnInput(ChatInputCallback cb) { on_input_ = std::move(cb); }

    void Update();
    void Draw() const;

    bool IsWindowOpen() const { return window_open_; }

private:
    void UpdateMessageVisibility();

    void InsertText(std::string_view text);
    void DeleteText(int count);
    void ClearText();
    void UpdateCharacterXs();
    float GetTextWidth() const;

    float GetCursorX() const;
    void MoveCursor(int offset);
    void ShiftTextToFitCursor();

    std::string ConvertInputToUTF8() const;
    void Submit();

    void Scroll(int dir);
    void ScrollHistory(int dir);

    void DrawOverlay() const;
    void DrawWindow() const;

private:
    Context& ctx_;
    const float& time_;

    std::vector<ChatMessage> messages_;
    size_t first_visible_idx_ = 0;

    bool window_open_ = false;
    glm::vec2 window_size_;

    bool block_input_ = false; // prevent T appearing after opening chat
    // std::string input_;
    std::vector<InputChar> line_;
    int cursor_pos_ = 0;
    float input_offset_ = 0.0f;

    ChatInputCallback on_input_;

    int scroll_ = 0;

    std::vector<std::string> history_;
    int current_history_idx_ = 0;
};

} // namespace gui