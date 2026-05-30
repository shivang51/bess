#include "bess_wgpu/wgpu_texture.h"
#include "bess_wgpu/wgpu_renderer_2d.h"

#include <stdexcept>

namespace Bess::Wgpu {

    WgpuTexture::WgpuTexture(WgpuRenderer2D &renderer)
        : m_renderer(&renderer) {}

    WgpuTexture::WgpuTexture(WgpuRenderer2D &renderer, const std::string &path)
        : ITexture(path),
          m_renderer(&renderer) {}

    WgpuTexture::WgpuTexture(
        WgpuRenderer2D &renderer,
        const Core::Renderer::TextureCreateInfo &createInfo)
        : ITexture(createInfo),
          m_renderer(&renderer) {}

    WgpuTexture::~WgpuTexture() { destroy(); }

    void WgpuTexture::init() {
        if (m_renderer == nullptr) {
            throw std::runtime_error("WgpuTexture has no renderer");
        }

        destroy();
        if (m_path.empty()) {
            if (m_size.x <= 0.f || m_size.y <= 0.f) {
                throw std::runtime_error(
                    "WgpuTexture size must be set for render-target textures");
            }

            m_handle = m_renderer->createRenderTarget(
                {.width = static_cast<uint32_t>(m_size.x),
                 .height = static_cast<uint32_t>(m_size.y)},
                m_renderer->getTargetFormatType());
            return;
        }

        m_handle = m_renderer->createTexture(*this);
    }

    void WgpuTexture::destroy() {
        if (m_renderer == nullptr || m_handle == 0) {
            m_handle = 0;
            return;
        }

        m_renderer->destroyTexture(m_handle);
        m_handle = 0;
    }

    void WgpuTexture::setRenderer(WgpuRenderer2D &renderer) noexcept {
        m_renderer = &renderer;
    }

} // namespace Bess::Wgpu
