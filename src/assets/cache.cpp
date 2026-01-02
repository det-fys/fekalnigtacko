#include "cache.hpp"

assets::SkeletonCache assets::CacheManager::skeleton_cache_;
assets::ModelCache assets::CacheManager::model_cache_;
assets::MapCache assets::CacheManager::map_cache_;
assets::VehicleCache assets::CacheManager::vehicle_cache_;

CLIENT_ONLY(assets::TextureCache assets::CacheManager::texture_cache_;)