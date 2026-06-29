#include "precache.hpp"

#include "cmdfile.hpp"
#include "cache.hpp"

assets::Precache::Precache(const std::string& path)
{
    LoadCMDFile(path, [this](const std::string& command, CmdLineStream& iss) {
        std::string name;
        iss >> name;
        items_.emplace_back(PrecacheItem{command, name});
    });
}

static std::any LoadItem(const assets::PrecacheItem& item)
{
    if (item.type == "fx")
        return assets::CacheManager::GetEffect("data/" + item.name + ".fx");
    else if (item.type == "font")
        return assets::CacheManager::GetFont("data/" + item.name + ".font");
    else if (item.type == "item")
        return assets::CacheManager::GetItem("data/" + item.name + ".item");
    else if (item.type == "map")
        return assets::CacheManager::GetMap("data/" + item.name + ".map");
    else if (item.type == "model")
        return assets::CacheManager::GetModel("data/" + item.name + ".mdl");
    else if (item.type == "skeleton")
        return assets::CacheManager::GetSkeleton("data/" + item.name + ".sk");
    else if (item.type == "sound")
        return assets::CacheManager::GetSound("data/" + item.name + ".snd");
    else if (item.type == "texture")
        return assets::CacheManager::GetTexture("data/" + item.name + ".png");
    else if (item.type == "vehicle")
        return assets::CacheManager::GetVehicleModel("data/" + item.name + ".veh");
    else
        throw std::runtime_error("Precache: invalid asset type: " + item.type);
}

void assets::Precache::LoadNext()
{
    if (IsDone())
        return;

    refs_.emplace_back(LoadItem(items_[num_loaded_]));
    ++num_loaded_;
}

