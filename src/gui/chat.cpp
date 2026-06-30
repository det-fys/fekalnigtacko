#include "chat.hpp"
#include "utils/utf8.hpp"

static constexpr float MESSAGE_DISPLAY_TIME = 20.0f;
static constexpr float MESSAGE_FADE_TIME = 1.0f;
static constexpr size_t MAX_DISPLAY_MESSAGES = 20;
static constexpr size_t MAX_DISPLAY_MESSAGES_WINDOW = 20;
static constexpr size_t MAX_INPUT_CHARS = 256;

static constexpr glm::vec2 MESSAGES_PADDING(10.0f);

gui::Chat::Chat(Context& ctx, const float& time) : ctx_(ctx), time_(time)
{
    window_size_.x = 1200.0f + MESSAGES_PADDING.x * 2.0f;
    window_size_.y = ctx_.GetFont()->GetLineHeight() * (MAX_DISPLAY_MESSAGES_WINDOW + 1) + MESSAGES_PADDING.y * 2.0f;
}

void gui::Chat::AddMessage(std::string message)
{
    auto& msg = messages_.emplace_back();
    msg.time = time_;
    msg.text = std::move(message);

    // limit number of displayed messages
    if (first_visible_idx_ + MAX_DISPLAY_MESSAGES < messages_.size())
    {
        ++first_visible_idx_;
    }
}

void gui::Chat::SetWindowOpen(bool open)
{
    if (open == window_open_)
        return;

    window_open_ = open;

    if (window_open_)
    {
        block_input_ = true;
        scroll_ = 0; // reset scroll when opening chat window
    }
}

bool gui::Chat::KeyInput(KeyCode key)
{
    if (!window_open_)
    {
        if (key == KEY_T)
        {
            SetWindowOpen(true);
            return true;
        }

        return false;
    }
    
    if (key == KEY_LCTRL || key == KEY_RMB)
    {
        SetWindowOpen(false);
        return true;
    }
    else if (key == KEY_LEFT)
    {
        MoveCursor(-1);
    }
    else if (key == KEY_RIGHT)
    {
        MoveCursor(1);
    }
    else if (key == KEY_BACKSPACE)
    {
        DeleteText(-1);
    }
    else if (key == KEY_DELETE)
    {
        DeleteText(1);
    }
    else if (key == KEY_ENTER)
    {
        Submit();
    }
    else if (key == KEY_UP)
    {
        ScrollHistory(-1);
    }
    else if (key == KEY_DOWN)
    {
        ScrollHistory(1);
    }
    else if (key == KEY_WHEELDOWN)
    {
        Scroll(-1);
    }
    else if (key == KEY_WHEELUP)
    {
        Scroll(1);
    }
    else if (key == KEY_PAGEDOWN)
    {
        Scroll(static_cast<int>(MAX_DISPLAY_MESSAGES_WINDOW / 2));
    }
    else if (key == KEY_PAGEUP)
    {
        Scroll(-static_cast<int>(MAX_DISPLAY_MESSAGES_WINDOW / 2));
    }

    return true; // intercept keys when chat open
}

void gui::Chat::TextInput(std::string_view text)
{
    if (!window_open_ || block_input_)
        return;

    InsertText(text);
}

void gui::Chat::Update()
{
    block_input_ = false;
    UpdateMessageVisibility();
}

void gui::Chat::Draw() const
{
    if (!window_open_)
    {
        DrawOverlay();
    }
    else
    {
        DrawWindow();
    }
}

void gui::Chat::UpdateMessageVisibility()
{
    for (size_t i = first_visible_idx_; i < messages_.size(); ++i)
    {
        auto& msg = messages_[i];
        if (time_ - msg.time >= MESSAGE_DISPLAY_TIME)
        {
            ++first_visible_idx_;
        }
    }
}

void gui::Chat::InsertText(std::string_view text)
{
    uint32_t cp;
    while ((cp = DecodeUTF8Codepoint(text)) && line_.size() < MAX_INPUT_CHARS)
    {
        InputChar ic{};
        ic.cp = cp;
        ic.width = ctx_.MeasureGlyph(cp);
        line_.insert(line_.begin() + cursor_pos_, ic);
        ++cursor_pos_;
    }

    UpdateCharacterXs();
}

void gui::Chat::DeleteText(int count)
{
    if (count == 0 || line_.empty())
        return;

    if (count < 0)
    {
        int delete_count = -count;
        if (delete_count > cursor_pos_)
            delete_count = cursor_pos_;
        if (delete_count <= 0)
            return;

        auto begin = line_.begin() + (cursor_pos_ - delete_count);
        auto end = line_.begin() + cursor_pos_;
        line_.erase(begin, end);
        cursor_pos_ -= delete_count;
    }
    else
    {
        int available = static_cast<int>(line_.size()) - cursor_pos_;
        if (available <= 0)
            return;

        int delete_count = count > available ? available : count;
        auto begin = line_.begin() + cursor_pos_;
        line_.erase(begin, begin + delete_count);
    }

    UpdateCharacterXs();
}

void gui::Chat::ClearText()
{
    line_.clear();
    cursor_pos_ = 0;
    ShiftTextToFitCursor();
}

void gui::Chat::UpdateCharacterXs()
{
    float x = 0.0f;
    for (auto& ic : line_)
    {
        ic.x = x;
        x += ic.width;
    }

    ShiftTextToFitCursor();
}

float gui::Chat::GetTextWidth() const
{
    if (line_.empty())
        return 0.0f;

    return line_.back().x + line_.back().width;
}

float gui::Chat::GetCursorX() const
{
    if (cursor_pos_ < line_.size())
        return line_[cursor_pos_].x;

    return GetTextWidth();
}

void gui::Chat::MoveCursor(int offset)
{
    cursor_pos_ = glm::clamp(cursor_pos_ + offset, 0, static_cast<int>(line_.size()));
    ShiftTextToFitCursor();
}

void gui::Chat::ShiftTextToFitCursor()
{
    float text_width = GetTextWidth();
    float cursor_x = GetCursorX();

    if (text_width <= window_size_.x)
    {
        input_offset_ = 0.0f;
        return;
    }

    if (cursor_x + input_offset_ < 0.0f)
    {
        input_offset_ = -cursor_x;
    }
    else if (cursor_x + input_offset_ > window_size_.x)
    {
        input_offset_ = window_size_.x - cursor_x;
    }

    input_offset_ = glm::clamp(input_offset_, window_size_.x - text_width, 0.0f);
}

std::string gui::Chat::ConvertInputToUTF8() const
{
    std::string str;
    for (const auto& ic : line_)
    {
        EncodeUTF8Codepoint(str, ic.cp);
    }

    return str;
}

void gui::Chat::Submit()
{
    std::string input = ConvertInputToUTF8();
    ClearText();
    SetWindowOpen(false);
    current_history_idx_ = 0;

    if (input.empty())
    {
        return;
    }

    if (history_.empty() || input != history_.back())
    {
        history_.emplace_back(input);
    }


    if (on_input_)
        on_input_(std::move(input));
}

void gui::Chat::Scroll(int dir)
{
    if (dir == 0)
        return;

    int num_messages = static_cast<int>(messages_.size());
    int max_scroll = 0;

    if (num_messages > static_cast<int>(MAX_DISPLAY_MESSAGES_WINDOW))
    {
        max_scroll = -(num_messages - static_cast<int>(MAX_DISPLAY_MESSAGES_WINDOW));
    }

    // limit scroll to visible window size and message count
    scroll_ = glm::clamp(scroll_ + dir, max_scroll, 0);
}

void gui::Chat::ScrollHistory(int dir)
{
    int new_history_idx = glm::clamp(current_history_idx_ + dir, -static_cast<int>(history_.size()), 0);

    if (new_history_idx == current_history_idx_)
        return;

    current_history_idx_ = new_history_idx;

    ClearText();

    if (current_history_idx_ >= 0)
        return;

    InsertText(history_[history_.size() + current_history_idx_]);
}

void gui::Chat::DrawOverlay() const
{
    for (size_t i = first_visible_idx_; i < messages_.size(); ++i)
    {
        auto& msg = messages_[i];

        glm::vec2 pos(10.0f, static_cast<float>(i - first_visible_idx_) * ctx_.GetFont()->GetLineHeight() + 10.0f);

        glm::vec4 color(1.0f);
        float t_rem = MESSAGE_DISPLAY_TIME - (time_ - msg.time);
        if (t_rem < MESSAGE_FADE_TIME)
        {
            color.a = t_rem / MESSAGE_FADE_TIME;
        }

        ctx_.DrawText(msg.text, pos, glm::packUnorm4x8(color));
    }
}

void gui::Chat::DrawWindow() const
{
    auto line_height = ctx_.GetFont()->GetLineHeight();

    constexpr glm::vec2 margin(30.0f);
    auto window_p0 = margin;
    auto window_p1 = window_p0 + window_size_;
    auto line_p0 = glm::vec2(window_p0.x, window_p1.y - line_height);
    auto line_p1 = window_p1;
    auto messages_p0 = window_p0 + MESSAGES_PADDING;
    auto messages_p1 = glm::vec2(window_p1.x, line_p0.y) - MESSAGES_PADDING;

    // background
    ctx_.DrawRect(window_p0, window_p1, 0x77000000);
    {
        ctx_.PushClipRect(window_p0, window_p1);

        // messages
        {
            ctx_.PushClipRect(messages_p0, messages_p1);
            float y = messages_p1.y;
            for (int i = messages_.size() - 1 + scroll_; i >= 0; --i)
            {
                if (y < messages_p0.y)
                    break;

                y -= line_height;

                auto pos = glm::vec2(messages_p0.x, y);
                ctx_.DrawText(messages_[i].text, pos);
            }

            ctx_.PopClipRect(); // messages
        }

        // input line
        ctx_.DrawRect(line_p0, line_p1, 0x77000000);
        {
            ctx_.PushClipRect(line_p0, line_p1);

            // text
            ctx_.BeginGlyphs();
            for (const auto& ic : line_)
            {
                glm::vec2 cursor(line_p0.x + input_offset_ + ic.x, line_p0.y);
                ctx_.DrawGlyph(cursor, ic.cp, 0xFFFFFFFF, 1.0f);
            }

            // cursor
            float cursor_x = GetCursorX() + input_offset_ + line_p0.x;
            auto cursor_p0 = glm::vec2(cursor_x - 1.0f, line_p0.y);
            auto cursor_p1 = glm::vec2(cursor_x + 1.0f, line_p1.y);
            ctx_.DrawRect(cursor_p0, cursor_p1, 0xAAFFFFFF);

            // ctx_.DrawText(input_, window_p0);
            ctx_.PopClipRect(); // line
        }

        ctx_.PopClipRect(); // window
    }
}
