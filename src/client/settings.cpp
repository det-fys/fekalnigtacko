#include "settings.hpp"

#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <iostream>

#include "net/inmessage.hpp"
#include "net/outmessage.hpp"

#include "utils/cvars.hpp"

static uint32_t settings_version = 1;

CVAR_CL(float, save_interval, CV_NONE, 1.0f, 0.0f);

static std::vector<char> LoadFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::binary | std::ios::ate);

    if (!file)
        return {}; // empty

    const std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(static_cast<size_t>(size));

    if (!file.read(buffer.data(), size))
        throw std::runtime_error("Failed to read file: " + filename);

    return buffer;
}

static void SaveFile(const std::string& filename, const std::vector<char>& data)
{
    std::ofstream file(filename, std::ios::binary);

    if (!file)
        throw std::runtime_error("Failed to open file for writing: " + filename);

    file.write(data.data(), static_cast<std::streamsize>(data.size()));

    if (!file)
        throw std::runtime_error("Failed to write file: " + filename);
}

Settings::Settings(const std::string& path) : path_(path) {}

using SaveKey = net::FixedStr<128>;
using SaveValue = net::FixedStr<1024>;

void Settings::Load() const
{
    auto data = LoadFile(path_);
    if (data.empty())
        return;

    net::InMessage msg(data.data(), data.size());

    uint32_t ver = 0;
    if (!msg.Read(ver) || ver != settings_version)
    {
        throw std::runtime_error("Invalid settings version: " + std::to_string(ver) + ", expected: " + std::to_string(settings_version));
    }

    while (!msg.Eof())
    {
        SaveKey key;
        SaveValue value;

        if (!msg.Read(key) || !msg.Read(value))
        {
            throw std::runtime_error("Error while reading settings key/value");
        }

        std::string key_str = key;
        std::string value_str = value;

        try
        {
            auto& cvar = CVarRegistry::GetClientInstance().GetCVar(key_str);
            cvar.SetString(value_str);
            cvar.ClearUnsaved(); // its literally saved
        }
        catch (const std::exception& e)
        {
            std::cerr << "settings: could not set '" << key_str << "':  " <<  e.what() << '\n';
        }
    }
    
}

void Settings::TrySave(float time)
{
    if (time - last_save_time_ < save_interval.Get())
        return;

    last_save_time_ = time;

    // check any unsaved cvars
    if (!CVarRegistry::GetClientInstance().ProcessCVars(nullptr, CV_SAVE | CV_UNSAVED))
        return;

    Save();
}

void Settings::Save() const
{
    std::vector<char> data;
    net::OutMessage msg(data);

    msg.Write(settings_version);

    CVarRegistry::GetClientInstance().ProcessCVars([&](CVarBase& cvar) {
        msg.Write(SaveKey(cvar.GetName()));
        msg.Write(SaveValue(cvar.GetString()));
        cvar.ClearUnsaved();
        return false; // continue
    }, CV_SAVE);

    SaveFile(path_, data);
}
