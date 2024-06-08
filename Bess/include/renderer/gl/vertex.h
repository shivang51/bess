#pragma once

#include "glm.hpp"
namespace Bess::Gl {

struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
    glm::vec2 texCoord;
    int id;
};

struct GridVertex {
    glm::vec3 position;
    glm::vec2 texCoord;
    int id;
    float ar;
};

struct QuadVertex {
    glm::vec3 position;
    glm::vec3 color;
    glm::vec2 texCoord;
    float borderSize;
    glm::vec4 borderRadius;
    glm::vec4 borderColor;
    float ar;
    int id;
};
} // namespace Bess::Gl
