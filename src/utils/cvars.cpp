#include "cvars.hpp"

#include <stdexcept>

CVarRegistry& CVarRegistry::GetClientInstance()
{
    static CVarRegistry reg;
    return reg;
}

CVarRegistry& CVarRegistry::GetServerInstance()
{
    static CVarRegistry reg;
    return reg;
}

void CVarRegistry::Register(const std::string& name, CVarBase* cvar)
{
    cvars_[name] = cvar;
}

void CVarRegistry::Set(const std::string& name, const std::string& val)
{
    auto& cvar = GetCVar(name);

    try
    {
        cvar.SetString(val);
    }
    catch (std::runtime_error& e)
    {
        throw std::runtime_error("Error setting " + name + ": " + std::string(e.what()));
    }
}

std::string CVarRegistry::Get(const std::string& name)
{
    return GetCVar(name).GetString();
}

bool CVarRegistry::ProcessCVars(std::function<bool(CVarBase&)> func, CVarFlags filter)
{
    const auto& cvars = cvars_;
    for (const auto& entry : cvars)
    {
        if (filter > 0 && (entry.second->GetFlags() & filter) != filter)
            continue;

        if (!func || func(*entry.second))
        {
            return true;
        }
    }

    return false;
}

bool CVarRegistry::IsCVarName(const std::string& name)
{
    return cvars_.contains(name);
}

CVarBase& CVarRegistry::GetCVar(const std::string& name)
{
    auto it = cvars_.find(name);
    if (it == cvars_.end())
    {
        throw std::runtime_error("Invalid cvar name: " + name);
    }

    return *it->second;
}
