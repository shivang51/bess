#pragma once

#include "glm.hpp"
namespace Bess::Gl {

    struct Vertex {
        glm::vec3 position;
        glm::vec4 color;
        glm::vec2 texCoord;
        int id;
    };

    struct GridVertex {
        glm::vec3 position;
        glm::vec2 texCoord;
        int id;
        glm::vec4 color;
        float ar;
    };

    struct RenderPassVertex {
        glm::vec3 position;
        glm::vec2 texCoord;
    };

    struct QuadVertex {
        glm::vec3 position;
        glm::vec4 color;
        glm::vec2 texCoord;
        glm::vec4 borderRadius;
        glm::vec4 borderColor;
        glm::vec4 borderSize;
        glm::vec2 size;
        int id;
        int isMica;
    };
} // namespace Bess::Gl
