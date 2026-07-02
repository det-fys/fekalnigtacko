#pragma once

#include <string>
#include <string_view>
#include <map>
#include <concepts>
#include <stdexcept>
#include <charconv>

#include <glm/glm.hpp>

using CVarFlags = uint8_t;
enum CVarFlag : CVarFlags
{
    CV_NONE = 0,

    CV_CONST = 1,
    CV_CONFIDENTIAL = 2,
    CV_SAVE = 4,

    CV_MODIFIED = 64,
    CV_UNSAVED = 128,
};

class CVarBase;

class CVarRegistry
{
public:
    static CVarRegistry& GetClientInstance();
    static CVarRegistry& GetServerInstance();

    void Register(const std::string& name, CVarBase* cvar);

    bool IsCVarName(const std::string& name);
    CVarBase& GetCVar(const std::string& name);
    void Set(const std::string& name, const std::string& val);
    std::string Get(const std::string& name);

    bool ProcessCVars(std::function<bool(CVarBase&)> func, CVarFlags filter = 0);

private:
    CVarRegistry() = default;

    std::map<std::string, CVarBase*> cvars_;
};

class CVarBase
{
public:
    CVarBase(bool client, const std::string& name, CVarFlags flags) : name_(name), flags_(flags)
    {
        (client ? CVarRegistry::GetClientInstance() : CVarRegistry::GetServerInstance()).Register(name, this);
    }

    virtual void SetString(const std::string& val) = 0;
    virtual std::string GetString() const = 0;

    const std::string& GetName() const { return name_; }
    const CVarFlags& GetFlags() const { return flags_; }

    bool IsConst() const { return flags_ & CV_CONST; }
    bool IsConfidential() const { return flags_ & CV_CONFIDENTIAL; }

    void ClearUnsaved() { flags_ &= ~CV_UNSAVED; }

    bool IsModified() const { return (flags_ & CV_MODIFIED) > 0; }
    void ClearModified() { flags_ &= ~CV_MODIFIED; }

protected:
    CVarFlags flags_;

private:
    std::string name_;
};

template <typename T>
concept CVarIntType = std::integral<T>;

template <typename T>
concept CVarFloatType = std::is_same_v<T, float>;

template <typename T>
concept CVarNumberType = CVarIntType<T> || CVarFloatType<T>;

template <typename T>
concept CVarStringType = std::is_same_v<T, std::string>;

template <typename T>
concept CVarType = CVarNumberType<T> || CVarStringType<T>;

template <typename T>
using CVarRangeType = std::conditional_t<CVarNumberType<T>, T, size_t>;

template <CVarType T>
class CVar : public CVarBase
{
public:
    CVar(bool client, const std::string& name, CVarFlags flags, const T& initial,
         CVarRangeType<T> min = std::numeric_limits<CVarRangeType<T>>().lowest(),
         CVarRangeType<T> max = std::numeric_limits<CVarRangeType<T>>().max())
        : CVarBase(client, name, flags), value_{}, min_(min), max_(max)
    {
        SetInternal(initial);
        flags_ |= CV_MODIFIED;
    }

    void Set(T value)
    {
        if (value == value_)
            return;

        SetInternal(value);
        flags_ |= CV_MODIFIED | CV_UNSAVED;
    }

    virtual void SetString(const std::string& value) override
    {
        if constexpr (CVarIntType<T>)
        {
            T v{};
            auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), v);

            if (ec != std::errc{})
                throw std::runtime_error("Invalid cvar value");
        
            Set(v);
        }
        else if constexpr (CVarFloatType<T>)
        {
            // std::from_chars for float exists only in newer standards / partial support,
            // so std::stof is still the pragmatic choice.
            Set(std::stof(value));
        }
        else // string
        {
            Set(value);
        }
    }

    const T& Get() const
    {
        return value_;
    }

    virtual std::string GetString() const override
    {
        if constexpr (CVarStringType<T>)
        {
            return value_;
        }
        else
        {
            return std::to_string(value_);
        }
    }

private:
    void SetInternal(T value)
    {
        if constexpr (CVarNumberType<T>)
        {
            value = glm::clamp(value, min_, max_);
        }
        else // string
        {
            if (value.size() < min_ || value.size() > max_)
            {
                throw std::runtime_error("Invalid cvar string value length");
            }
        }

        value_ = std::move(value);
    }

private:
    T value_;
    CVarRangeType<T> min_, max_;
};

#define CVAR(type, name, ...) static CVar<type> name{false, #name, __VA_ARGS__}
#define CVAR_CL(type, name, ...) static CVar<type> name{true, #name, __VA_ARGS__}
