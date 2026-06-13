#include "bess_core/asset_manager/asset_manager.h"

namespace Bess::Assets {
    void AssetManager::onDestroy() {
        for (auto &[typeIdx, cacheAny] : m_assetCaches) {
            auto assets = std::any_cast<
                std::unordered_map<uint64_t, std::shared_ptr<void>> &>(
                cacheAny);

            // LOG num of ref counts for each asset before clearing
            for (const auto &[id, assetPtr] : assets) {
                BESS_TRACE("[AssetManager] Asset of type {} with ID {} has {} "
                           "references",
                           typeIdx.name(),
                           id,
                           assetPtr.use_count());
            }
        }
    }

    void AssetManager::onInit() {
        m_assetCaches.clear();
    }

    void AssetManager::onShutdown() {
        m_assetCaches.clear();
    }

} // namespace Bess::Assets
