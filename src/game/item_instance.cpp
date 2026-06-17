#include "item_instance.hpp"
#include "assets/cache.hpp"

game::ItemInstance::ItemInstance(std::shared_ptr<const assets::Item> def) : def(std::move(def))
{
    ammo = this->def->clip_size; // full clip by default
}

game::ItemInstance::ItemInstance(const std::string& name)
    : ItemInstance(assets::CacheManager::GetItem("data/" + name + ".item"))
{
}
