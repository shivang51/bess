#pragma once

#include "common/bess_api.h"
#include "controls/render_view.h"
#include "ui_painter.h"
#include "ui_style.h"
#include "widget.h"

#include <memory>
#include <optional>

namespace Bess::UI {

    class SceneView;

    struct SceneViewTargetOptions {
        Core::Renderer::Renderer2DExtent initialExtent{1, 1};
        std::optional<Core::Renderer::Renderer2DTargetFormat> colorFormat;
        std::optional<Core::Renderer::Renderer2DTargetFormat> pickingFormat;
    };

    struct SceneViewUpdateContext {
        SceneView &view;
        WidgetBounds bounds;
        TimeMs deltaTime;
        Core::Renderer::Renderer2DExtent extent{0, 0};
        glm::vec2 treeViewportSize{0.f, 0.f};
        bool hasTargets = false;
    };

    struct SceneViewFrameContext {
        SceneView &view;
        Core::Renderer::IRenderer2D &renderer;
        WidgetBounds bounds;
        Core::Renderer::Renderer2DExtent extent{0, 0};
        Core::Renderer::TextureHandle colorTarget = 0;
        Core::Renderer::TextureHandle pickingTarget = 0;
        std::shared_ptr<Core::Renderer::ITexture> colorTexture;
        std::shared_ptr<Core::Renderer::ITexture> pickingTexture;
        TimeMs deltaTime;
        glm::vec2 treeViewportSize{0.f, 0.f};
        bool effectivelyVisible = false;
        bool resized = false;
    };

    class BESS_API ISceneViewDelegate {
      public:
        virtual ~ISceneViewDelegate();

        virtual void onAttach(SceneView &view);
        virtual void onDetach(SceneView &view) noexcept;
        virtual void update(SceneViewUpdateContext &context);
        virtual void render(SceneViewFrameContext &context) = 0;
        virtual UIEventReply onEvent(SceneView &view,
                                     WidgetEventContext &context,
                                     const UIEvent &event);
        [[nodiscard]] virtual CursorIcon
        cursor(const SceneView &view,
               const WidgetCursorContext &context) const noexcept;
    };

    struct SceneViewOptions {
        RenderPolicy policy = RenderPolicy::whileVisible;
        SceneViewTargetOptions target;
        float renderScale = 1.f;
        Core::Renderer::Color tint{1.f, 1.f, 1.f, 1.f};
        glm::vec4 cornerRadius{0.f};
        bool focusable = true;
        bool hitTestVisible = true;
        std::optional<UIBoxStyle> placeholder;
    };

    class BESS_API SceneView final : public Widget {
      public:
        explicit SceneView(std::shared_ptr<ISceneViewDelegate> delegate,
                           SceneViewOptions options = {});

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

        [[nodiscard]] Core::Renderer::Renderer2DExtent extent() const noexcept;
        [[nodiscard]] std::shared_ptr<Core::Renderer::ITexture>
        colorTexture() const;
        [[nodiscard]] std::shared_ptr<Core::Renderer::ITexture>
        pickingTexture() const;
        [[nodiscard]] Core::Renderer::TextureHandle colorHandle() const;
        [[nodiscard]] Core::Renderer::TextureHandle pickingHandle() const;

        bool requestPickingId(uint32_t x, uint32_t y);
        bool requestPickingIds(uint32_t x,
                               uint32_t y,
                               uint32_t width,
                               uint32_t height);
        [[nodiscard]] bool
        tryGetPickingIds(Core::Renderer::PickingReadbackResult &result) const;
        [[nodiscard]] PickingId readPickingId(uint32_t x, uint32_t y) const;

        [[nodiscard]] const std::shared_ptr<ISceneViewDelegate> &
        delegate() const noexcept;
        void setDelegate(std::shared_ptr<ISceneViewDelegate> delegate);

      private:
        void destroyTarget() noexcept;
        void ensureTarget(const std::shared_ptr<Core::Renderer::IRenderer2D>
                              &renderer);
        [[nodiscard]] bool resizeTarget(Core::Renderer::Renderer2DExtent extent);
        void synchronizeDelegate();
        [[nodiscard]] Core::Renderer::Renderer2DExtent
        desiredExtent(WidgetBounds bounds, float contentScale) const noexcept;
        [[nodiscard]] bool shouldRender(bool effectivelyVisible,
                                        bool resized) const noexcept;

        SceneViewOptions m_options;
        std::shared_ptr<Core::Renderer::IRenderer2D> m_renderer;
        std::shared_ptr<Core::Renderer::IRenderTarget2D> m_target;
        std::shared_ptr<ISceneViewDelegate> m_delegate;
        std::shared_ptr<ISceneViewDelegate> m_attachedDelegate;
        WidgetTree *m_state = nullptr;
        WidgetId m_id;
        bool m_renderRequested = true;
        bool m_synchronizingDelegate = false;
    };

} // namespace Bess::UI
