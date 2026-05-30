#pragma once

#include "common/sub_system.h"
#include "pages/main_page/main_page.h"
#include "bess_wgpu/wgpu_texture.h"

namespace Bess {
    class UISubSystem : public Bess::ISubSystem {
      public:
        void onInit() override;
        void onPostInit() override;
        void onShutdown() override;
        void onDestroy() override;

        void onPreDraw() override;
        void onPostDraw() override;
        void onDraw() override;
        void onUpdate(TimeMs dt) override;

      private:
        int m_currentFps = 0;
        std::shared_ptr<Pages::MainPage> m_mainPage = nullptr;
        std::shared_ptr<Wgpu::WgpuTexture> m_previewTex = nullptr;
    };

} // namespace Bess
