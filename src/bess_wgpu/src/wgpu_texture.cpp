#include "bess_wgpu/wgpu_texture.h"
#include "bess_wgpu/wgpu_renderer_2d.h"
#include "common/bess_assert.h"
#include "common/logger.h"
#include "stb_image.h"

#include <cstdint>
#include <stdexcept>

namespace Bess::Wgpu {

    std::shared_ptr<WgpuRenderer2D> WgpuTexture::s_renderer = nullptr;

    void WgpuTexture::setRenderer(
        const std::shared_ptr<WgpuRenderer2D> &renderer) noexcept {
        s_renderer = renderer;
    }

    std::shared_ptr<WgpuTexture> WgpuTexture::fromPixels(const uint8_t *pixels,
                                                         uint32_t width,
                                                         uint32_t height) {
        BESS_ASSERT(s_renderer != nullptr, "WgpuTexture has no renderer");
        auto texture = std::make_shared<WgpuTexture>();
        texture->createTextureFromPixels(pixels, width, height);
        texture->setSize(
            {static_cast<float>(width), static_cast<float>(height)});
        texture->m_handle = getNextTextureHandle();
        texture->m_textureView = texture->m_wgpuHandle.CreateView();
        s_renderer->registerTexture(texture->getResource());
        return texture;
    }

    WgpuTexture::WgpuTexture(const std::string &path) : ITexture(path) {}

    WgpuTexture::WgpuTexture(
        const Core::Renderer::TextureCreateInfo &createInfo)
        : ITexture(createInfo) {}

    WgpuTexture::~WgpuTexture() { destroy(); }

    void WgpuTexture::init() {
        if (s_renderer == nullptr) {
            throw std::runtime_error("WgpuTexture has no renderer");
        }

        destroy();
        if (m_path.empty()) {
            if (m_size.x <= 0.f || m_size.y <= 0.f) {
                throw std::runtime_error(
                    "WgpuTexture size must be set for render-target textures");
            }
            initRenderTarget();
        } else {
            initTexture();
        }

        m_handle = getNextTextureHandle();
        m_textureView = m_wgpuHandle.CreateView();
        s_renderer->registerTexture(getResource());
    }

    void WgpuTexture::destroy() {
        if (s_renderer == nullptr || m_handle == 0) {
            m_handle = 0;
            m_wgpuHandle = nullptr;
            return;
        }

        m_wgpuHandle = nullptr;
        s_renderer->unregisterTexture(m_handle);
        m_handle = 0;
    }
    void *WgpuTexture::getView() const {
        return (void *)getTextureView().Get();
    }

    wgpu::TextureView WgpuTexture::getTextureView() const {
        BESS_ASSERT(m_wgpuHandle != nullptr, "Texture not initialized");
        BESS_ASSERT(m_textureView != nullptr,
                    "Texture view is not initialized");
        return m_textureView;
    }

    TextureResource WgpuTexture::getResource() const {
        TextureResource resource;
        resource.handle = m_handle;
        resource.view = getTextureView();
        resource.texture = m_wgpuHandle;
        if (!m_isRenderTarget) {
            resource.width = (uint32_t)m_size.x;
            resource.height = (uint32_t)m_size.y;
        }
        return resource;
    }

    struct StbiImageDeleter {
        void operator()(stbi_uc *pixels) const { stbi_image_free(pixels); }
    };

    void WgpuTexture::initTexture() {
        BESS_ASSERT(s_renderer != nullptr, "WgpuTexture has no renderer");

        auto device = s_renderer->getDevice();

        int width = 0;
        int height = 0;
        int channels = 0;
        std::unique_ptr<stbi_uc, StbiImageDeleter> pixels(
            stbi_load(m_path.c_str(), &width, &height, &channels, 4));

        if (pixels == nullptr) {
            throw std::runtime_error("Failed to load texture: " + m_path +
                                     " - " + stbi_failure_reason());
        }

        createTextureFromPixels(pixels.get(), static_cast<uint32_t>(width),
                                static_cast<uint32_t>(height));
        setSize({static_cast<float>(width), static_cast<float>(height)});
    }

    void WgpuTexture::initRenderTarget() {
        wgpu::TextureDescriptor descriptor{};
        descriptor.dimension = wgpu::TextureDimension::e2D;
        descriptor.size = {std::max(1u, (uint32_t)m_size.x),
                           std::max(1u, (uint32_t)m_size.y), 1};
        descriptor.format = s_renderer->getTargetFormat();
        descriptor.mipLevelCount = 1;
        descriptor.sampleCount = 1;
        descriptor.usage = wgpu::TextureUsage::RenderAttachment |
                           wgpu::TextureUsage::TextureBinding |
                           wgpu::TextureUsage::CopySrc;
        descriptor.label = "RenderTargetTexture";

        m_wgpuHandle = s_renderer->getDevice().CreateTexture(&descriptor);
        m_isRenderTarget = true;
    }

    void WgpuTexture::createTextureFromPixels(const uint8_t *pixels,
                                              uint32_t width, uint32_t height) {
        if (pixels == nullptr || width == 0 || height == 0) {
            throw std::runtime_error("Invalid texture pixel data");
        }

        wgpu::TextureDescriptor descriptor{};
        descriptor.dimension = wgpu::TextureDimension::e2D;
        descriptor.size = {width, height, 1};
        descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
        descriptor.mipLevelCount = 1;
        descriptor.sampleCount = 1;
        descriptor.usage =
            wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;

        auto device = s_renderer->getDevice();
        m_wgpuHandle = device.CreateTexture(&descriptor);

        wgpu::TexelCopyTextureInfo destination{};
        destination.texture = device.CreateTexture(&descriptor);
        destination.mipLevel = 0;
        destination.origin = {0, 0, 0};
        destination.aspect = wgpu::TextureAspect::All;

        wgpu::TexelCopyBufferLayout layout{};
        layout.offset = 0;
        layout.bytesPerRow = width * 4;
        layout.rowsPerImage = height;

        wgpu::Extent3D writeSize{width, height, 1};
        s_renderer->getQueue().WriteTexture(
            &destination, pixels, static_cast<size_t>(width) * height * 4,
            &layout, &writeSize);
        m_isRenderTarget = false;
    }

} // namespace Bess::Wgpu
