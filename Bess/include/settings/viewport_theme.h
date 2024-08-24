#pragma once
#include "glm.hpp"

namespace Bess {
class ViewportTheme {
  public:
    static glm::vec3 componentBGColor;
    static glm::vec3 selectedCompColor;
    static glm::vec4 componentBorderColor;
    static glm::vec3 backgroundColor;
    static glm::vec4 stateHighColor;
    static glm::vec4 wireColor;
    static glm::vec3 selectedWireColor;
    static glm::vec3 compHeaderColor;
    static void updateColorsFromImGuiStyle();
};
} // namespace Bess
