#pragma once

#include "model.hpp"
#include "skeleton.hpp"
#include "gfx/texture.hpp"

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

        PtrType obj = Load(key);
        cache_[key] = obj; // Cache the loaded object
        return obj;
    }

protected:
    virtual PtrType Load(const std::string& key) = 0;

private:
    std::map<std::string, std::weak_ptr<const T>> cache_;
};

class TextureCache final : public Cache<gfx::Texture>
{
protected:
    PtrType Load(const std::string& key) override { return gfx::Texture::LoadFromFile(key); }
};

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

class CacheManager
{
public:
    static std::shared_ptr<const gfx::Texture> GetTexture(const std::string& filename) {
        return texture_cache_.Get(filename);
    }

    static std::shared_ptr<const Skeleton> GetSkeleton(const std::string& filename) {
        return skeleton_cache_.Get(filename);
    }

    static std::shared_ptr<const Model> GetModel(const std::string& filename) {
        return model_cache_.Get(filename);
    }

private:
    static TextureCache texture_cache_;
    static SkeletonCache skeleton_cache_;
    static ModelCache model_cache_;

};

} // namespace assets