#pragma once
#include "common/class_helpers.h"
#include "events/application_event.h"
#include "glm.hpp"
#include "imgui.h"
#include "scene.h"
#include "string"
#include "ui_panel.h"
#include "viewport.h"
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

        void destroyViewport();

        void update(TimeMs ts, const std::vector<ApplicationEvent> &events) override;

        const glm::vec2 &getViewportSize() const;
        const glm::vec2 &getViewportPos() const;
        bool isHovered() const;

        MAKE_GETTER_SETTER(std::shared_ptr<Canvas::Viewport>, Viewport, m_viewport);
        MAKE_GETTER_SETTER_WC(std::shared_ptr<Canvas::Scene>,
                              AttachedScene,
                              m_attachedScene,
                              onSceneAttached);

      private:
        void firstTime();
        void drawTopLeftControls() const;
        void drawBottomControls() const;

        void onSceneAttached();

        VkExtent2D vec2Extent2D(const glm::vec2 &vec);

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
        std::shared_ptr<Canvas::Viewport> m_viewport;
        bool m_isResized = false;
        std::shared_ptr<Canvas::Scene> m_attachedScene;
    };
} // namespace Bess::UI
