#pragma once

#include "map.hpp"
#include "model.hpp"
#include "skeleton.hpp"
#include "vehiclemdl.hpp"
#include "item.hpp"

#include "utils/defs.hpp"

#ifdef CLIENT
#include "audio/sound.hpp"
#include "gfx/texture.hpp"
#include "gui/font.hpp"
#include "effect.hpp"
#endif

#include <iostream>

namespace assets
{

template <typename T>
class Cache
{
public:
    using PtrType = std::shared_ptr<const T>;

    PtrType Get(const std::string& key)
    {
        auto it = cache_.find(key);
        if (it != cache_.end())
        {
            if (auto ptr = it->second.lock())
            {
                return ptr; // Return cached object
            }
        }

        std::cout << "loading " << key << "..." << std::endl;
        PtrType obj = Load(key);
        cache_[key] = obj; // Cache the loaded object
        return obj;
    }

protected:
    virtual PtrType Load(const std::string& key) = 0;

private:
    std::map<std::string, std::weak_ptr<const T>> cache_;
};

#ifdef CLIENT
class TextureCache final : public Cache<gfx::Texture>
{
protected:
    PtrType Load(const std::string& key) override { return gfx::Texture::LoadFromFile(key); }
};

class SoundCache final : public Cache<audio::Sound>
{
protected:
    PtrType Load(const std::string& key) override { return audio::Sound::LoadFromFile(key); }
};

class FontCache final : public Cache<gui::Font>
{
protected:
    PtrType Load(const std::string& key) override { return gui::Font::LoadFromFile(key); }
};

class EffectCache final : public Cache<Effect>
{
protected:
    PtrType Load(const std::string& key) override { return Effect::LoadFromFile(key); }
};
#endif // CLIENT

class SkeletonCache final : public Cache<Skeleton>
{
protected:
    PtrType Load(const std::string& key) override { return Skeleton::LoadFromFile(key); }
};

class ModelCache final : public Cache<Model>
{
protected:
    PtrType Load(const std::string& key) override { return Model::LoadFromFile(key); }
};

class MapCache final : public Cache<Map>
{
protected:
    PtrType Load(const std::string& key) override { return Map::LoadFromFile(key); }
};

class VehicleCache final : public Cache<VehicleModel>
{
protected:
    PtrType Load(const std::string& key) override { return VehicleModel::LoadFromFile(key); }
};

class ItemCache final : public Cache<Item>
{
protected:
    PtrType Load(const std::string& key) override { return Item::LoadFromFile(key); }
};

class CacheManager
{
public:
    static std::shared_ptr<const Skeleton> GetSkeleton(const std::string& filename)
    {
        return skeleton_cache_.Get(filename);
    }

    static std::shared_ptr<const Model> GetModel(const std::string& filename) { return model_cache_.Get(filename); }

    static std::shared_ptr<const Map> GetMap(const std::string& filename) { return map_cache_.Get(filename); }

    static std::shared_ptr<const VehicleModel> GetVehicleModel(const std::string& filename)
    {
        return vehicle_cache_.Get(filename);
    }

    static std::shared_ptr<const Item> GetItem(const std::string& filename)
    {
        return item_cache_.Get(filename);
    }

#ifdef CLIENT
    static std::shared_ptr<const gfx::Texture> GetTexture(const std::string& filename)
    {
        return texture_cache_.Get(filename);
    }

    static std::shared_ptr<const audio::Sound> GetSound(const std::string& filename)
    {
        return sound_cache_.Get(filename);
    }

    static std::shared_ptr<const gui::Font> GetFont(const std::string& filename)
    {
        return font_cache_.Get(filename);
    }
    
    static std::shared_ptr<const Effect> GetEffect(const std::string& filename)
    {
        return effect_cache_.Get(filename);
    }
#endif

private:
    static SkeletonCache skeleton_cache_;
    static ModelCache model_cache_;
    static MapCache map_cache_;
    static VehicleCache vehicle_cache_;
    static ItemCache item_cache_;
    CLIENT_ONLY(static TextureCache texture_cache_;)
    CLIENT_ONLY(static SoundCache sound_cache_;)
    CLIENT_ONLY(static FontCache font_cache_;)
    CLIENT_ONLY(static EffectCache effect_cache_;)
};

} // namespace assets