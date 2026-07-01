#pragma once

#include <map>
#include <mutex>

#include "utils/defs.hpp"

#include <iostream>

namespace assets
{

class Asset
{
public:
    Asset() = default;
    DELETE_COPY_MOVE(Asset);

    const std::string& GetAssetName() const { return name_; }

    virtual ~Asset() = default;

private:
    friend class AssetManager;

    std::string name_;
};

struct AssetIdentifier
{
    size_t type_hash;
    std::string name;

    bool operator<(const AssetIdentifier& other) const
    {
        return std::tie(type_hash, name) < std::tie(other.type_hash, other.name);
    }
};

template <typename T>
concept AnyAsset = std::derived_from<T, Asset> && requires(const std::string& s) {
    { T::Load(s) } -> std::convertible_to<std::shared_ptr<T>>;
};

class AssetManager
{
public:
    static AssetManager& GetInstance();

    template <AnyAsset T>
    std::shared_ptr<const T> Get(const std::string& name)
    {
        // LOCK
        std::lock_guard<std::recursive_mutex> lock(mtx_);

        // create identifier
        AssetIdentifier aid{};
        aid.type_hash = typeid(T).hash_code();
        aid.name = name;

        // check already loaded
        if (auto ptr = assets_[aid].lock())
        {
            // already loaded
            return std::dynamic_pointer_cast<T>(ptr);
        }

        // load
        std::cout << "loading <" << typeid(T).name() << "> \"" << name << "\"" << std::endl;
        auto ptr = T::Load(name);
        static_cast<Asset*>(ptr.get())->name_ = name;

        // store weak ptr
        assets_[aid] = ptr;

        return ptr;
    }

private:
    AssetManager() = default;

private:
    std::recursive_mutex mtx_;
    std::map<AssetIdentifier, std::weak_ptr<Asset>> assets_;
};

} // namespace assets
