#pragma once

#include <string>

class Settings
{
public:
    Settings(const std::string& path);

    void Load() const;

    void TrySave(float time);
    void Save() const;

private:
    std::string path_;
    float last_save_time_ = 0.0f;

};