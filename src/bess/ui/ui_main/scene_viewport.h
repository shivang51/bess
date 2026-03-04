#pragma once
#include "glm.hpp"
#include "imgui.h"
#include "string"
namespace Bess::UI {
    class SceneViewport {
      public:
        SceneViewport(const std::string &viewportName);
        ~SceneViewport() = default;
        void draw();

        const glm::vec2 &getViewportSize() const;
        const glm::vec2 &getViewportPos() const;
        bool isHovered() const;

      private:
        void firstTime();
        void drawTopLeftControls() const;
        void drawBottomControls() const;

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
        bool m_isHovered;
    };
} // namespace Bess::UI
