#pragma once

#include "bess_uuid.h"
#include "glm.hpp"
#include <vector>

namespace Bess::Renderer2D::Vulkan {
    struct PathPoint {
        glm::vec3 pos;
        float weight = 1.f;
        int64_t id = 0.f;
    };
}; // namespace Bess::Renderer2D::Vulkan

namespace Bess::Renderer {
    class Path {
      public:
        Path() = default;

        struct MoveTo {
            glm::vec2 p;
        };

        struct LineTo {
            glm::vec2 p;
        };

        struct QuadTo {
            glm::vec2 c, p;
        };

        struct CubicTo {
            glm::vec2 c1, c2, p;
        };

        struct PathCommand {
            enum class Kind : uint8_t { Move,
                                        Line,
                                        Quad,
                                        Cubic } kind;
            union {
                MoveTo move;
                LineTo line;
                QuadTo quad;
                CubicTo cubic;
            };

            float z = 1.f;
            float weight = 1.f;
            int64_t id = -1;
        };

        Path *moveTo(const glm::vec2 &pos);

        Path *lineTo(const glm::vec2 &pos);

        Path *quadTo(const glm::vec2 &c, const glm::vec2 &pos);

        Path *cubicTo(const glm::vec2 &c1, const glm::vec2 &c2, const glm::vec2 &pos);

        Path *addCommand(PathCommand cmd);

        const std::vector<PathCommand> &getCmds() const;

        void setCommands(const std::vector<PathCommand> &cmds);

        const std::vector<std::vector<Renderer2D::Vulkan::PathPoint>> &getContours();

        UUID uuid;

      private:
        void formContours();

      private:
        std::vector<PathCommand> m_cmds;
        std::vector<std::vector<Renderer2D::Vulkan::PathPoint>> m_contours;
    };
} // namespace Bess::Renderer
