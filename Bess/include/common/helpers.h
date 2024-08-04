#pragma once

#include "glm.hpp"

namespace Bess::Common {
    class Helpers {
    public:
        static glm::vec3 GetLeftCornerPos(const glm::vec3& pos, const glm::vec2& size);
    };
}