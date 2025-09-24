#pragma once
#include "component_types/component_types.h"
#include "glm.hpp"
#include <unordered_map>

namespace Bess {

    struct SceneColors {
        glm::vec4 background;
        glm::vec4 grid;

        glm::vec4 compHeader;
        glm::vec4 componentBG;
        glm::vec4 componentBorder;
        glm::vec4 selectedComp;

        glm::vec4 wire;
        glm::vec4 ghostWire;
        glm::vec4 selectedWire;
        glm::vec4 clockConnectionLow;
        glm::vec4 clockConnectionHigh;

        glm::vec4 text;

        glm::vec4 selectionBoxBorder;
        glm::vec4 selectionBoxFill;

        glm::vec4 stateHigh;
        glm::vec4 stateLow;
    };

    class ViewportTheme {
      public:
        static SceneColors colors;
        static void updateColorsFromImGuiStyle();

        static glm::vec4 getCompHeaderColor(Bess::SimEngine::ComponentType type);

      private:
        static void initCompColorMap();
        static std::unordered_map<Bess::SimEngine::ComponentType, glm::vec4> s_compHeaderColorMap;
    };
} // namespace Bess
