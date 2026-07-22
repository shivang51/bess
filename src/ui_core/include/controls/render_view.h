#pragma once

#include "common/bess_api.h"
#include "render_surface.h"
#include "ui_painter.h"
#include "widget.h"

#include <memory>

namespace Bess::UI {

    class RenderView;

    enum class RenderPolicy : uint8_t {
        // Render after requestRender(), on first presentation, or resize.
        onDemand,
        // Render every target frame while effectively visible.
        whileVisible,
        // Render every target frame while mounted, including when hidden.
        continuous,
    };

    struct RenderViewUpdateContext {
        RenderView &view;
        RenderSurface &surface;
        WidgetBounds bounds;
        TimeMs deltaTime;
    };

    struct RenderViewFrameContext {
        RenderView &view;
        RenderSurface &surface;
        Core::Renderer::IRenderer2D &renderer;
        WidgetBounds bounds;
        Core::Renderer::Renderer2DExtent extent;
        TimeMs deltaTime;
        bool effectivelyVisible = false;
    };

    // Renderer-facing behavior/controller for a RenderView. Scene-specific
    // camera, picking, and input state belongs in implementations of this
    // interface, not in the generic widget or surface. RenderView owns target
    // resize and frame begin/end: configureFrame() may edit the supplied frame
    // description and render() may only enqueue draw commands. Neither may
    // recursively render, resize, or sample from the active surface.
    class BESS_API IRenderViewDelegate {
      public:
        virtual ~IRenderViewDelegate();

        virtual void onAttach(RenderView &view);
        virtual void onDetach(RenderView &view) noexcept;
        virtual void update(RenderViewUpdateContext &context);
        virtual void
        configureFrame(RenderViewFrameContext &context,
                       Core::Renderer::RenderTarget2DFrameInfo &frameInfo);
        virtual void render(RenderViewFrameContext &context) = 0;
        virtual UIEventReply onEvent(RenderView &view,
                                     WidgetEventContext &context,
                                     const UIEvent &event);
        [[nodiscard]] virtual CursorIcon
        cursor(const RenderView &view,
               const WidgetCursorContext &context) const noexcept;
    };

    struct RenderViewOptions {
        RenderPolicy policy = RenderPolicy::whileVisible;
        RenderSurfaceOptions surface;
        Core::Renderer::RenderTarget2DFrameInfo frame{
            .clearColor = {0.f, 0.f, 0.f, 1.f},
            .shouldClear = true,
        };
        // Multiplied by UITarget's content scale before physical attachment
        // dimensions are rounded. Useful for deliberate supersampling.
        float renderScale = 1.f;
        Core::Renderer::Color tint{1.f, 1.f, 1.f, 1.f};
        glm::vec4 cornerRadius{0.f};
        bool focusable = true;
        bool hitTestVisible = true;
    };

    // Interactive presentation of an offscreen RenderSurface. Offscreen work
    // occurs only in prepareRender(), never paint(), so renderer targets are
    // never nested. paint() merely composites the last completed color image.
    // A surface accepts one mounted RenderView producer; share it with other
    // controls only as a passive, dynamically reacquired texture source.
    class BESS_API RenderView final : public Widget {
      public:
        explicit RenderView(std::shared_ptr<IRenderViewDelegate> delegate,
                            RenderViewOptions options = {},
                            std::shared_ptr<RenderSurface> surface = {});

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;
        void onUnmount(WidgetTree &state, WidgetId id) override;
        void update(WidgetUpdateContext &context) override;
        void prepareRender(WidgetRenderPrepareContext &context) override;
        void paint(WidgetPaintContext &context) const override;
        UIEventReply onEvent(WidgetEventContext &context,
                             const UIEvent &event) override;
        [[nodiscard]] CursorIcon
        cursor(const WidgetCursorContext &context) const noexcept override;

        void requestRender() noexcept;
        [[nodiscard]] bool renderRequested() const noexcept;
        [[nodiscard]] RenderPolicy renderPolicy() const noexcept;
        void setRenderPolicy(RenderPolicy policy) noexcept;
        [[nodiscard]] const std::shared_ptr<RenderSurface> &
        surface() const noexcept;
        void setSurface(std::shared_ptr<RenderSurface> surface);
        [[nodiscard]] const std::shared_ptr<IRenderViewDelegate> &
        delegate() const noexcept;
        void setDelegate(std::shared_ptr<IRenderViewDelegate> delegate);

      private:
        void synchronizeDelegate();
        [[nodiscard]] Core::Renderer::Renderer2DExtent
        desiredExtent(WidgetBounds bounds, float contentScale) const noexcept;
        [[nodiscard]] bool shouldRender(bool effectivelyVisible,
                                        bool resized) const noexcept;

        RenderViewOptions m_options;
        std::shared_ptr<RenderSurface> m_surface;
        std::shared_ptr<IRenderViewDelegate> m_delegate;
        // The configured delegate and the delegate which has received
        // onAttach() are tracked separately. Lifecycle callbacks may replace
        // the configured delegate re-entrantly without detaching an object
        // which was never attached or recursively detaching itself.
        std::shared_ptr<IRenderViewDelegate> m_attachedDelegate;
        WidgetTree *m_state = nullptr;
        WidgetId m_id;
        bool m_renderRequested = true;
        bool m_synchronizingDelegate = false;
    };

} // namespace Bess::UI
