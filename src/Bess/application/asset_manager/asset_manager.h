#pragma once
#include "asset_id.h"
#include "asset_loader.h"
#include "common/log.h"
#include "scene/renderer/asset_loaders.h"
#include <any>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>

/// Note (Shivang)
/// Doing this to remove copies of singleton across different shared library boundaries.
/// This is needed for the asset manager to cleanup properly across plugins and main application. :)
#if defined(_WIN32) // still need to check on windows (this should not work)
    #define API_EXPORT __declspec(dllexport)
#else
    #define API_EXPORT __attribute__((visibility("default")))
#endif

namespace Bess::Assets {
    class API_EXPORT AssetManager {
      public:
        static AssetManager &instance();

        AssetManager(const AssetManager &) = delete;
        AssetManager &operator=(const AssetManager &) = delete;
        AssetManager(AssetManager &&) = delete;
        AssetManager &operator=(AssetManager &&) = delete;
        ~AssetManager() = default;

      private:
        AssetManager() = default;

      public:
        template <typename T, size_t N>
        std::shared_ptr<T> get(const AssetID<T, N> &id) {
            const auto typeIdx = std::type_index(typeid(T));

            using AssetCacheT = std::unordered_map<uint64_t, std::shared_ptr<void>>;

            auto it = m_assetCaches.find(typeIdx);
            if (it == m_assetCaches.end()) {
                it = m_assetCaches.emplace(typeIdx, AssetCacheT{}).first;
            }

            auto &cache = std::any_cast<AssetCacheT &>(it->second);

            auto assetIt = cache.find(id.id);
            if (assetIt != cache.end()) {
                return std::static_pointer_cast<T>(assetIt->second);
            }

            BESS_INFO("[AssetManager] Loading {} from 1st path {} with ID {}", typeid(T).name(), id.paths[0], id.id);

            std::shared_ptr<T> asset = std::apply(
                [](auto &&...paths) {
                    return AssetLoader<T>::load(std::string(paths)...);
                },
                id.paths);

            if (asset) {
                cache[id.id] = asset;
            } else {
                BESS_WARN("[AssetManager] Asset was returned as nullptr");
            }

            return asset;
        }

        void clear() {
            m_assetCaches.clear();
            BESS_INFO("[AssetManager] Cache Cleared");
        }

      private:
        std::unordered_map<std::type_index, std::any> m_assetCaches;
    };
} // namespace Bess::Assets
