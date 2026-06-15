#pragma once
#include "bess_core/renderer/texture.h"
#include "common/bess_uuid.h"
#include "common/class_helpers.h"
#include "common/types.h"
#include "imgui.h"
#include "scene.h"
#include "scene_types.h"
#include "string"
#include "ui_panel.h"

namespace Bess {
    struct MouseButtonState;
} // namespace Bess

namespace Bess::UI {
    class SceneViewportPanel : public Panel {
      public:
        SceneViewportPanel(const std::string &viewportName);

        void renderAttachedScene();

        void onBeforeDraw() override;
        void onDraw() override;
        void onAfterDraw() override;

        void init() override;
        void destroy() override;

        void update(TimeMs ts) override;

        const glm::vec2 &getViewportSize() const;
        const glm::vec2 &getViewportPos() const;
        bool isHovered() const;
        bool isFocused() const;
        bool
        isAttachedToScene(const std::shared_ptr<Canvas::Scene> &scene) const;
        void focusCameraOnSelected();

        std::shared_ptr<Camera> getCamera() const;

        MAKE_GETTER_SETTER_WC(std::shared_ptr<Canvas::Scene>,
                              AttachedScene,
                              m_attachedScene,
                              onSceneAttached);

        MAKE_GETTER(size_t, ViewportId, m_viewportId)
        MAKE_GETTER(PickingId, PickingId, m_pickingId)

      private:
        void updateScene(TimeMs ts);

        void updatePickingIds(bool mouseMoved);

        void handleMouseMove(const glm::vec2 &mousePos);
        void releaseMouseButtonOutsideViewport(
            const MouseButtonState &mouseBtnState);
        void applySceneCursor();

        bool isInsideViewport(const glm::vec2 &pos) const;
        bool hasRenderableViewport() const;
        bool hasMouseCapture() const;

      private:
        struct PendingPickingReadback {
            uint32_t x = 0;
            uint32_t y = 0;
            uint32_t width = 0;
            uint32_t height = 0;
            bool active = false;

            bool matches(uint32_t otherX,
                         uint32_t otherY,
                         uint32_t otherWidth,
                         uint32_t otherHeight) const {
                return active && x == otherX && y == otherY &&
                       width == otherWidth && height == otherHeight;
            }

            void clear() {
                *this = {};
            }
        };

        void firstTime();
        void drawTopLeftControls();
        void drawBottomControls() const;

        void onSceneAttached();

        glm::vec2 getSceneMousePos();

        static constexpr ImGuiWindowFlags NO_MOVE_FLAGS =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoDecoration;

      private:
        bool m_isfirstTimeDraw;
        glm::vec2 m_viewportSize = {800.f, 600.f};
        glm::vec2 m_viewportPos;
        ImVec2 m_localPos;
        std::string m_viewportName;
        bool m_isResized = false;
        UUID m_nextSceneId = UUID::null;
        PickingId m_pickingId = PickingId::invalid();
        Canvas::SceneInputState m_inputState;
        std::shared_ptr<Canvas::Scene> m_attachedScene;
        std::shared_ptr<Core::Renderer::ITexture> m_sceneTexture = nullptr;
        std::shared_ptr<Core::Renderer::ITexture> m_pickingTexture = nullptr;
        std::shared_ptr<Camera> m_camera = nullptr;
        std::vector<const Canvas::SceneState *> m_rootToSceneStatePtrs;
        uint32_t m_gridShader = 0;
        PendingPickingReadback m_pendingSelectionReadback;
        size_t m_viewportId = 0;
    };
} // namespace Bess::UI
