#include "common/helpers.h"

namespace Bess::Common {
    glm::vec3 Helpers::GetLeftCornerPos(const glm::vec3& pos, const glm::vec2& size) {
        return { pos.x - size.x / 2, pos.y + size.y / 2, pos.z };
    }
}