#pragma once
#include "uuid_v4.h"
#include "glm.hpp"

namespace Bess::Common {
    class Helpers {
    public:
        static glm::vec3 GetLeftCornerPos(const glm::vec3& pos, const glm::vec2& size);
        static UUIDv4::UUIDGenerator<std::mt19937_64> uuidGenerator;
    };
}