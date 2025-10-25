#pragma once

#include "bess_uuid.h"
#include "glm.hpp"
#include <vector>

namespace Bess::Renderer2D::Vulkan {
    struct PathPoint {
        glm::vec3 pos;
        float weight = 1.f;
        int64_t id = 0.f;
        glm::vec4 color = glm::vec4(-1.f);
    };
}; // namespace Bess::Renderer2D::Vulkan

namespace Bess::Renderer {
    struct PathProperties {
        bool isClosed = false;
        bool roundedJoints = false;
        bool renderStroke = true;
        bool renderFill = false;
    };

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

            float z = 0.f;
            float weight = 1.f;
            int64_t id = -1;
        };

        Path *moveTo(const glm::vec2 &pos);

        Path *lineTo(const glm::vec2 &pos);

        Path *quadTo(const glm::vec2 &c, const glm::vec2 &pos);

        Path *cubicTo(const glm::vec2 &c1, const glm::vec2 &c2, const glm::vec2 &pos);

        Path *addCommand(PathCommand cmd);

        const std::vector<PathCommand> &getCmds() const;

        bool scale(const glm::vec2 &factor);

        void setCommands(const std::vector<PathCommand> &cmds);

        void setProps(const PathProperties &props);

        const PathProperties &getProps() const;

        PathProperties &getPropsRef();

        const std::vector<std::vector<Renderer2D::Vulkan::PathPoint>> &getContours();

        UUID uuid;

      private:
        void formContours();

      private:
        PathProperties m_props{};
        glm::vec2 m_currentScale = glm::vec2(1.f);
        std::vector<PathCommand> m_cmds;
        std::vector<std::vector<Renderer2D::Vulkan::PathPoint>> m_contours;
    };
} // namespace Bess::Renderer
