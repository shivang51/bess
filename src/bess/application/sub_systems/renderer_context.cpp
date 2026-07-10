#include "sub_systems/renderer_context.h"
#include "window.h"
#include "bess_core/g_app_context.h"
#include "bess_core/scene/scene_ui/controls/image_comp.h"
#include "bess_wgpu/wgpu_renderer_2d.h"
#include "bess_wgpu/wgpu_texture.h"
#include "common/events.h"
#include "event_dispatcher.h"

namespace Bess {

    void RendererContext::onPreInit() {
        m_renderer = std::make_shared<Wgpu::WgpuRenderer2D>();
        Wgpu::WgpuTexture::setRenderer(
            std::dynamic_pointer_cast<Wgpu::WgpuRenderer2D>(m_renderer));
    }

    void RendererContext::onInit() {
        auto &ctx = GAppContext::getInstance();
        const auto &window = ctx.getSubSystem<Window>();

        m_renderer->init(
            {.extent = {800, 600},
             .targetFormat = Core::Renderer::Renderer2DTargetFormat::BGRA8Unorm,
             .pickingFormat = Core::Renderer::Renderer2DTargetFormat::RG32Uint,
             .surface = {.type = Core::Renderer::Renderer2DNativeSurfaceType::
                             PlatformHandle,
                         .handle = window->getGLFWHandle()}});

        Canvas::UI::ImageComp::setDefaultTextureLoader(
            [](const Canvas::UI::UIImageSourceRequest &request)
                -> std::shared_ptr<Core::Renderer::ITexture> {
                if (request.type == Canvas::UI::UIImageSourceType::File) {
                    auto texture =
                        std::make_shared<Wgpu::WgpuTexture>(request.path);
                    texture->init();
                    return texture;
                }

                if (request.type == Canvas::UI::UIImageSourceType::Pixels) {
                    const auto &pixels = request.pixels;
                    if (pixels.rgba8.empty() || pixels.width == 0u ||
                        pixels.height == 0u) {
                        return nullptr;
                    }
                    return Wgpu::WgpuTexture::fromPixels(
                        pixels.rgba8.data(), pixels.width, pixels.height);
                }

                return nullptr;
            });
    }

    void RendererContext::onPostInit() {
        auto &ctx = GAppContext::getInstance();
        auto evtDispatcher = ctx.getSubSystem<EventSystem::EventDispatcher>();

        evtDispatcher->sink<Events::WindowResizeEvent>().connect(
            [this](const Events::WindowResizeEvent &event) {
                m_renderer->resize(
                    {.width = event.width, .height = event.height});
            });
    }

    void RendererContext::onDestroy() {
        Canvas::UI::ImageComp::setDefaultTextureLoader(nullptr);
        Wgpu::WgpuTexture::setRenderer(nullptr);
        m_renderer->destroy();
        m_renderer.reset();
    }

} // namespace Bess
