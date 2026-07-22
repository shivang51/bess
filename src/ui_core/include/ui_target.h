#pragma once

#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/style/color_scheme.h"
#include "common/types.h"
#include "ui_event.h"
#include "ui_view.h"
#include "widget_tree.h"
#include <concepts>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace Bess::Core::Style {
    class BessTheme;
}

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
        // A target receives a BessTheme rather than an independent UI
        // palette. UICore maps its component roles from this source theme.
        std::shared_ptr<const Core::Style::BessTheme> theme;
        Core::Renderer::Renderer2DTargetFormat targetFormat =
            Core::Renderer::Renderer2DTargetFormat::BGRA8Unorm;
        Core::Renderer::Renderer2DTargetFormat pickingFormat =
            Core::Renderer::Renderer2DTargetFormat::RG32Uint;
    };

    // Rendering/input boundary for one UI surface. The surface may be native or
    // offscreen. WidgetTree owns retained UI state; optional features such as a
    // DockSpace live in that tree rather than being built into every target.
    class UITarget {
      public:
        UITarget();
        ~UITarget();

        void init(const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer,
                  const UITargetDesc &desc);
        void destroy();
        void setTheme(const Core::Style::BessTheme &theme);

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
        [[nodiscard]] CursorIcon getCursorShape() const noexcept;
        [[nodiscard]] WidgetTree &getWidgetTree() noexcept;
        [[nodiscard]] const WidgetTree &getWidgetTree() const noexcept;
        [[nodiscard]] UIViewHost &getViewHost() noexcept;
        [[nodiscard]] const UIViewHost &getViewHost() const noexcept;

        UIViewRef<UIView> setContent(std::unique_ptr<UIView> view);
        UIViewRef<UIView> mountOverlay(std::unique_ptr<UIView> view);
        UIViewRef<UIView> mountModal(std::unique_ptr<UIView> view);

        template <typename T, typename... Args>
            requires std::derived_from<T, UIView> &&
                     std::constructible_from<T, Args...>
        UIViewRef<T> setContent(Args &&...args) {
            return m_viewHost.setContent<T>(std::forward<Args>(args)...);
        }

        template <typename T, typename... Args>
            requires std::derived_from<T, UIView> &&
                     std::constructible_from<T, Args...>
        UIViewRef<T> mountOverlay(Args &&...args) {
            return m_viewHost.mountOverlay<T>(std::forward<Args>(args)...);
        }

        template <typename T, typename... Args>
            requires std::derived_from<T, UIView> &&
                     std::constructible_from<T, Args...>
        UIViewRef<T> mountModal(Args &&...args) {
            return m_viewHost.mountModal<T>(std::forward<Args>(args)...);
        }

        bool unmountView(ViewId id);

        void resize(const glm::vec2 &size);

        void draw();

        void update(TimeMs dt);

      private:
        void beginFrame(const Core::Renderer::Color &background);
        void processInputEvents();

      private:
        WidgetTree m_widgetTree;
        UIViewHost m_viewHost;
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
