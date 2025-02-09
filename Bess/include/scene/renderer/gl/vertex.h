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

    struct QuadVertex {
        glm::vec3 position;
        glm::vec4 color;
        glm::vec2 texCoord;
        glm::vec4 borderRadius;
        glm::vec2 size;
        int id;
        int isMica;
    };
} // namespace Bess::Gl
