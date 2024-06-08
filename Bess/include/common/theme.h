#pragma once
#include "glm.hpp"

namespace Bess {
class Theme {
  public:
    static const glm::vec3 componentBGColor;
    static const glm::vec3 selectedCompColor;
    static const glm::vec4 componentBorderColor;
    static const glm::vec3 backgroundColor;
    static const glm::vec3 wireColor;
    static const glm::vec3 selectedWireColor;
    static const glm::vec3 compHeaderColor;
};
} // namespace Bess
