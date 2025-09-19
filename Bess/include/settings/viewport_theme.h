#pragma once
#include "component_types/component_types.h"
#include "ext/vector_float4.hpp"
#include <unordered_map>

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
        static glm::vec4 selectionBoxBorderColor;
        static glm::vec4 selectionBoxFillColor;
        static glm::vec4 stateLowColor;
        static void updateColorsFromImGuiStyle();

        static glm::vec4 getCompHeaderColor(Bess::SimEngine::ComponentType type);

      private:
        static void initCompColorMap();
        static std::unordered_map<Bess::SimEngine::ComponentType, glm::vec4> s_compColorMap;
    };
} // namespace Bess
