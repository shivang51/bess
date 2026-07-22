#pragma once

#include "common/bess_api.h"
#include "common/bess_uuid.h"
#include "common/class_helpers.h"
#include "common/types.h"
#include "controls/scene_view.h"
#include "bess_core/scene/camera.h"
#include "bess_core/scene/scene.h"
#include "bess_core/viewport.h"

#include <memory>
#include <string>

namespace Bess::UI {

    // Retained-mode scene viewport controller for SceneView.
    // Owns camera + ViewportContext, drives Scene::draw through texture
    // handles, and bridges InputSubSystem + UI events into the existing
    // scene interaction pipeline without nested renderer frames.
    class BESS_API SceneViewportController final
        : public ISceneViewDelegate,
          public std::enable_shared_from_this<SceneViewportController> {
      public:
        explicit SceneViewportController(std::string name = "Scene Viewport");
        ~SceneViewportController() override;

        SceneViewportController(const SceneViewportController &) = delete;
        SceneViewportController &
        operator=(const SceneViewportController &) = delete;

        void onAttach(SceneView &view) override;
        void onDetach(SceneView &view) noexcept override;
        void update(SceneViewUpdateContext &context) override;
        void render(SceneViewFrameContext &context) override;
        UIEventReply onEvent(SceneView &view,
                             WidgetEventContext &context,
                             const UIEvent &event) override;
        [[nodiscard]] CursorIcon
        cursor(const SceneView &view,
               const WidgetCursorContext &context) const noexcept override;

        [[nodiscard]] const std::string &name() const noexcept;
        [[nodiscard]] bool isHovered() const noexcept;
        [[nodiscard]] bool isFocused() const noexcept;
        [[nodiscard]] bool isUsable() const noexcept;
        [[nodiscard]] bool
        isAttachedToScene(const std::shared_ptr<Canvas::Scene> &scene) const;
        void focusCameraOnSelected();
        [[nodiscard]] bool isSchematicMode() const noexcept;
        bool toggleSchematicMode();
        void updateAttachedSceneId(const UUID &sceneId);

        MAKE_GETTER_SETTER_WC(std::shared_ptr<Canvas::Scene>,
                              AttachedScene,
                              m_attachedScene,
                              onSceneAttached);
        MAKE_GETTER(std::shared_ptr<Camera>, Camera, m_camera)
        MAKE_GETTER(std::shared_ptr<Core::Viewport::ViewportContext>,
                    ViewportContext,
                    m_viewportCtx)
        [[nodiscard]] size_t getViewportId() const noexcept {
            return m_viewportCtx ? m_viewportCtx->viewportId : 0;
        }
        [[nodiscard]] PickingId getPickingId() const noexcept {
            return m_viewportCtx ? m_viewportCtx->inputCtx.pickingId
                                 : PickingId::invalid();
        }
        [[nodiscard]] const glm::vec2 &getViewportSize() const noexcept {
            return m_viewportCtx->transform.size;
        }
        [[nodiscard]] const glm::vec2 &getViewportPos() const noexcept {
            return m_viewportCtx->transform.pos;
        }

      private:
        struct PendingPickingReadback {
            uint32_t x = 0;
            uint32_t y = 0;
            uint32_t width = 0;
            uint32_t height = 0;
            bool active = false;

            [[nodiscard]] bool matches(uint32_t otherX,
                                       uint32_t otherY,
                                       uint32_t otherWidth,
                                       uint32_t otherHeight) const noexcept {
                return active && x == otherX && y == otherY &&
                       width == otherWidth && height == otherHeight;
            }

            void clear() noexcept {
                *this = {};
            }
        };

        void onSceneAttached();
        void ensureCamera(const glm::vec2 &size);
        void syncAttachedScene();
        void applyGeometry(const WidgetBounds &bounds,
                           Core::Renderer::Renderer2DExtent extent);
        [[nodiscard]] bool hasRenderableViewport() const noexcept;
        [[nodiscard]] bool hasMouseCapture() const noexcept;
        void processInteraction(TimeMs ts, bool effectivelyVisible);
        void updatePickingIds(bool mouseMoved);
        void handleEdgePan(const glm::vec2 &windowMouse);
        void releaseMouseButtonOutsideViewport(
            const Input::MouseButtonEvent &mouseBtnState);
        [[nodiscard]] bool isInsideViewportWindow(const glm::vec2 &windowPos)
            const noexcept;
        [[nodiscard]] glm::vec2 windowSpaceSize() const noexcept;
        [[nodiscard]] static CursorIcon
        mapSceneCursor(Core::Viewport::SceneCursor cursor) noexcept;

        std::string m_name;
        SceneView *m_view = nullptr;
        std::shared_ptr<Core::Viewport::ViewportContext> m_viewportCtx;
        std::shared_ptr<Camera> m_camera;
        std::shared_ptr<Canvas::Scene> m_attachedScene;
        PendingPickingReadback m_pendingSelectionReadback;
        WidgetBounds m_bounds{};
        bool m_hovered = false;
        bool m_pointerInside = false;
        UUID m_nextSceneId = UUID::null;
        bool m_geometryDirty = true;
    };

} // namespace Bess::UI
