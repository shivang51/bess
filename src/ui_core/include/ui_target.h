#pragma once

#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/style/color_scheme.h"
#include "common/types.h"
#include "dock.h"
#include "layout.h"
#include "ui_event.h"
#include <memory>
#include <span>
#include <vector>

namespace Bess::UI {

    struct UITargetInpCtx {
        PickingId pickingId = PickingId::invalid();
        glm::vec2 mousePos = {0, 0};
        glm::vec2 mouseDelta = {0, 0};
        glm::vec2 mouseWheelDelta = {0, 0};
        Input::Modifiers modifiers;
    };

    struct Rect {
        glm::vec2 pos = {0, 0};
        glm::vec2 size = {0, 0};
    };

    struct UITargetDesc {
        Rect rect;
        Core::Renderer::Renderer2DNativeSurface surface;
        Core::Renderer::Renderer2DTargetFormat targetFormat =
            Core::Renderer::Renderer2DTargetFormat::BGRA8Unorm;
        Core::Renderer::Renderer2DTargetFormat pickingFormat =
            Core::Renderer::Renderer2DTargetFormat::RG32Uint;
    };

    // This will be a root UIContainer
    // and ideally it will draw directly to the window surface.
    // The surface is optional so the target can also draw offscreen.
    // It will have:
    // - its own picking texture
    // - a UINodes (layout engine nodes) container
    // - a dock manager
    // - and ui widgets
    class UITarget {
      public:
        UITarget() = default;
        ~UITarget();

        void init(const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer,
                  const UITargetDesc &desc);
        void destroy();

        [[nodiscard]] std::shared_ptr<Core::Renderer::ITexture>
        getColorTexture() const;
        [[nodiscard]] std::shared_ptr<Core::Renderer::ITexture>
        getPickingTexture() const;

        MAKE_GETTER_SETTER(Rect, Rect, m_rect);

        // Events enqueued before update() are exposed, in order, for that
        // frame. Events enqueued while a frame is being processed are held for
        // the following frame.
        void enqueueEvent(UIEvent event);
        void enqueueEvent(Input::Event event);
        template <typename T>
            requires(!std::same_as<std::remove_cvref_t<T>, UIEvent> &&
                     std::constructible_from<UIEventData, T>)
        void enqueueEvent(T &&event, Input::Modifiers eventModifiers = {}) {
            enqueueEvent(UIEvent{std::forward<T>(event), eventModifiers});
        }
        [[nodiscard]] std::span<const UIEvent> getFrameEvents() const noexcept;
        [[nodiscard]] const UITargetInpCtx &getInputContext() const noexcept;

        void resize(const glm::vec2 &size);

        void draw();

        void update(TimeMs dt);

      private:
        void beginFrame(const Core::Style::Color &background);
        void processInputEvents();

      private:
        DockManager m_dockManager;
        LayoutNodeRegistry m_layoutNodesReg;
        std::shared_ptr<Core::Renderer::IRenderer2D> m_renderer = nullptr;
        std::shared_ptr<Core::Renderer::IRenderTarget2D> m_renderTarget =
            nullptr;
        Rect m_rect{};
        UITargetInpCtx m_inputCtx;
        bool m_hasMousePos = false;
        std::vector<UIEvent> m_pendingEvents;
        std::vector<UIEvent> m_frameEvents;
    };
} // namespace Bess::UI
