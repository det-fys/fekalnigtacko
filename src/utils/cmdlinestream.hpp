#pragma once

#include <cctype>
#include <charconv>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

class CmdLineStream
{
public:
    explicit CmdLineStream(std::string_view line) : view_(line) {}

    bool Eol()
    {
        SkipWhitespace();
        return view_.empty() || view_.front() == '#';
    }

    bool Read(std::string& out)
    {
        out.clear();

        if (Eol())
        {
            return false;
        }

        if (view_.front() == '"')
        {
            return ParseQuoted(out);
        }

        size_t end = 0;

        while (end < view_.size())
        {
            char c = view_[end];

            if (std::isspace(static_cast<unsigned char>(c)) || c == '#')
                break;

            ++end;
        }

        out.assign(view_.substr(0, end));
        view_.remove_prefix(end);

        return true;
    }

    template <typename T>
        requires std::is_integral_v<T>
    bool Read(T& value)
    {
        SkipWhitespace();

        auto result = std::from_chars(view_.data(), view_.data() + view_.size(), value);

        if (result.ec != std::errc{})
            return false;

        view_.remove_prefix(static_cast<size_t>(result.ptr - view_.data()));

        return true;
    }

    bool Read(float& value)
    {
        SkipWhitespace();

        auto result = std::from_chars(view_.data(), view_.data() + view_.size(), value);

        if (result.ec != std::errc{})
            return false;

        view_.remove_prefix(static_cast<size_t>(result.ptr - view_.data()));

        return true;
    }

    template <typename T>
    CmdLineStream& operator>>(T& out)
    {
        if (!Read(out))
        {
            throw std::runtime_error("Invalid value");
        }

        return *this;
    }

private:
    void SkipWhitespace()
    {
        while (!view_.empty())
        {
            if (std::isspace(static_cast<unsigned char>(view_.front())))
            {
                view_.remove_prefix(1);
            }
            else
            {
                break;
            }
        }
    }

    bool ParseQuoted(std::string& out)
    {
        // remove opening quote
        view_.remove_prefix(1);

        while (!view_.empty())
        {
            char c = view_.front();
            view_.remove_prefix(1);

            if (c == '"')
                return true;

            if (c != '\\')
            {
                out.push_back(c);
                continue;
            }

            if (view_.empty())
                break;

            char escaped = view_.front();
            view_.remove_prefix(1);

            switch (escaped)
            {
            case 'n':
                out.push_back('\n');
                break;

            case 't':
                out.push_back('\t');
                break;

            case 'r':
                out.push_back('\r');
                break;

            case '\\':
                out.push_back('\\');
                break;

            case '"':
                out.push_back('"');
                break;

            default:
                // Keep unknown escapes literally
                out.push_back('\\');
                out.push_back(escaped);
                break;
            }
        }

        return false;
    }

private:
    std::string_view view_;
};
