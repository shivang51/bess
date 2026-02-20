#include "asset_manager/asset_manager.h"

namespace Bess::Assets {
    AssetManager &AssetManager::instance() {
        static AssetManager instance;
        return instance;
    }
} // namespace Bess::Assets
