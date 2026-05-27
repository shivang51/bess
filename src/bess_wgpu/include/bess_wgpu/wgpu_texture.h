#pragma once

#include "bess_core/renderer/texture.h"

namespace Bess::Wgpu {

    class WgpuRenderer2D;

    class WgpuTexture final : public Core::Renderer::ITexture {
      public:
        explicit WgpuTexture(WgpuRenderer2D &renderer);
        WgpuTexture(WgpuRenderer2D &renderer, const std::string &path);
        WgpuTexture(WgpuRenderer2D &renderer,
                    const Core::Renderer::TextureCreateInfo &createInfo);
        ~WgpuTexture() override;

        WgpuTexture(const WgpuTexture &) = delete;
        WgpuTexture &operator=(const WgpuTexture &) = delete;
        WgpuTexture(WgpuTexture &&) = delete;
        WgpuTexture &operator=(WgpuTexture &&) = delete;

        void init() override;
        void destroy() override;

        void setRenderer(WgpuRenderer2D &renderer) noexcept;

      private:
        WgpuRenderer2D *m_renderer = nullptr;
    };

} // namespace Bess::Wgpu
