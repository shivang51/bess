#pragma once
#include "glm.hpp"
#include <string>
#include <unordered_map>

namespace Bess {

    struct SceneColors {
        glm::vec4 background;

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
        glm::vec4 stateHighZ = glm::vec4(0.60f, 0.60f, 0.90f, 1.0f);
        glm::vec4 stateUnknow = glm::vec4(0.90f, 0.45f, 0.45f, 1.0f);

        glm::vec4 gridMinorColor;
        glm::vec4 gridMajorColor;
        glm::vec4 gridAxisXColor;
        glm::vec4 gridAxisYColor;

        glm::vec4 error = glm::vec4(0.95f, 0.25f, 0.25f, 1.0f);
    };

    struct SchematicViewColors {
        glm::vec4 pin = glm::vec4(0.25f, 0.85f, 0.75f, 1.0f);
        glm::vec4 text = glm::vec4(0.90f, 0.92f, 0.95f, 1.0f);
        glm::vec4 connection = glm::vec4(0.55f, 0.80f, 0.45f, 1.0f);
        glm::vec4 componentFill = glm::vec4(0.12f, 0.14f, 0.18f, 1.0f);
        glm::vec4 componentStroke = glm::vec4(0.30f, 0.35f, 0.45f, 1.0f);
    };

    class ViewportTheme {
      public:
        static SceneColors colors;
        static SchematicViewColors schematicViewColors;
        static void updateColorsFromImGuiStyle();

        static glm::vec4 getCompHeaderColor(const std::string &group);

      private:
        static void initCompColorMap();
        static std::unordered_map<std::string, glm::vec4> s_compHeaderColorMap;
    };
} // namespace Bess
