#pragma once

#include "common/sub_system.h"
#include "vulkan_core.h"

namespace Bess {
    class UISubSystem : public Bess::ISubSystem {
      public:
        void onInit() override;
        void onPostInit() override;
        void onShutdown() override;
        void onDestroy() override;

        void onDraw() override;
        void onUpdate(TimeMs dt) override;

      private:
        int m_currentFps = 0;
        std::shared_ptr<Vulkan::VulkanCore> m_vkCore = nullptr;
    };

} // namespace Bess
