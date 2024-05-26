#pragma once

#include "glm.hpp"
namespace Bess::Gl {
class Vertex {
public:
    glm::vec3 position;
    glm::vec3 color;
    glm::vec2 texCoord;
    int id;
};

struct QuadVertex: public Vertex {
public:
    glm::vec3 position;
    glm::vec3 color;
    glm::vec2 texCoord;
    int id;
    glm::vec4 borderRadius;
    float ar;
};
} // namespace Bess::Gl
