#include "precache.hpp"

#include "cmdfile.hpp"
#include "asset_manager.hpp"

#include "assets/effect.hpp"
#include "assets/item.hpp"
#include "assets/map.hpp"
#include "assets/model.hpp"
#include "assets/skeleton.hpp"
#include "assets/vehiclemdl.hpp"
#include "audio/sound.hpp"
#include "gfx/texture.hpp"
#include "gui/font.hpp"

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
    auto& am = assets::AssetManager::GetInstance();

    if (item.type == "fx")
        return am.Get<assets::Effect>(item.name);
    else if (item.type == "font")
        return am.Get<gui::Font>(item.name);
    else if (item.type == "item")
        return am.Get<assets::Item>(item.name);
    else if (item.type == "map")
        return am.Get<assets::Map>(item.name);
    else if (item.type == "model")
        return am.Get<assets::Model>(item.name);
    else if (item.type == "skeleton")
        return am.Get<assets::Skeleton>(item.name);
    else if (item.type == "sound")
        return am.Get<audio::Sound>(item.name);
    else if (item.type == "texture")
        return am.Get<gfx::Texture>(item.name);
    else if (item.type == "vehicle")
        return am.Get<assets::VehicleModel>(item.name);
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

