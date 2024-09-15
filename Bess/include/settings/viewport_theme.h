#pragma once
#include "ext/vector_float4.hpp"
#include "glm.hpp"

namespace Bess {
    class ViewportTheme {
      public:
        static glm::vec4 componentBGColor;
        static glm::vec4 selectedCompColor;
        static glm::vec4 componentBorderColor;
        static glm::vec4 backgroundColor;
        static glm::vec4 stateHighColor;
        static glm::vec4 wireColor;
        static glm::vec4 selectedWireColor;
        static glm::vec4 compHeaderColor;
        static glm::vec4 textColor;
        static glm::vec4 gridColor;
        static void updateColorsFromImGuiStyle();
    };
} // namespace Bess
