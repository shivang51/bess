#pragma once
#include "glm.hpp"
#include "string"
#include <cstdint>
namespace Bess::UI {
    class SceneViewport {
      public:
        SceneViewport(const std::string &viewportName);
        ~SceneViewport() = default;
        void draw();

        const glm::vec2 &getViewportSize() const;
        const glm::vec2 &getViewportPos() const;
        bool isHovered() const;
        void setViewportTexture(uint64_t texture);

      private:
        void firstTime();
        bool m_isfirstTimeDraw;
        uint64_t m_viewportTexture;
        glm::vec2 m_viewportSize = {800.f, 600.f};
        glm::vec2 m_viewportPos;
        std::string m_viewportName;
        bool m_isHovered;
    };
} // namespace Bess::UI
