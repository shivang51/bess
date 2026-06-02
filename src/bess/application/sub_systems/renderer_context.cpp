#include "sub_systems/renderer_context.h"
#include "application/window.h"
#include "bess_core/g_app_context.h"
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
             .surface = {.type = Core::Renderer::Renderer2DNativeSurfaceType::
                             PlatformHandle,
                         .handle = window->getGLFWHandle()}});
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
        Wgpu::WgpuTexture::setRenderer(nullptr);
        m_renderer->destroy();
        m_renderer.reset();
    }

} // namespace Bess
