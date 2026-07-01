#include "asset_manager.hpp"

assets::AssetManager& assets::AssetManager::GetInstance()
{
    static AssetManager manager;
    return manager;
}
