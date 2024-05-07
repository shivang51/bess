#pragma once

#include "glm.hpp"
namespace Bess::Gl {
struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
    glm::vec2 texCoord;
    int id;
};





struct QuadVertex {
    glm::vec3 position;
    glm::vec3 color;
    glm::vec2 texCoord;
    int id;
    glm::vec4 borderRadius;
    float ar;
};
} // namespace Bess::Gl
