#pragma once
#include "bess_core/renderer/renderer_types.h"
#include <string>
#include <unordered_map>

namespace Bess {
    using Color = Bess::Core::Renderer::Color;

    struct SceneColors {
        Color background;

        Color compHeader;
        Color componentBG;
        Color componentBorder;
        Color selectedComp;
        Color moduleColor;

        Color wire;
        Color ghostWire;
        Color selectedWire;
        Color clockConnectionLow;
        Color clockConnectionHigh;

        Color groupColor = Color::fromRGBA8(188, 151, 76, 255);
        Color text;

        Color selectionBoxBorder;
        Color selectionBoxFill;

        Color stateHigh;
        Color stateLow;
        Color stateHighZ = Color(0.60f, 0.60f, 0.90f, 1.0f);
        Color stateUnknow = Color(0.90f, 0.45f, 0.45f, 1.0f);

        Color gridMinorColor;
        Color gridMajorColor;
        Color gridAxisXColor;
        Color gridAxisYColor;

        Color error = Color(0.95f, 0.25f, 0.25f, 1.0f);
    };

    struct SchematicViewColors {
        Color pin = Color(0.20f, 0.75f, 0.85f, 1.0f);
        Color text = Color(0.85f, 0.85f, 0.85f, 1.0f);
        Color connection = Color(0.00f, 0.80f, 0.40f, 1.0f);
        Color componentFill = Color(0.08f, 0.09f, 0.11f, 1.0f);
        Color componentStroke = Color(0.45f, 0.50f, 0.60f, 1.0f);
        Color activeSignal = Color(1.00f, 0.90f, 0.20f, 1.0f);
    };

    struct SceneWidgetsColors {
        Color surface = Color(0.12f, 0.12f, 0.13f, 0.96f);
        Color surfaceHover = Color(0.16f, 0.16f, 0.17f, 0.98f);
        Color surfaceActive = Color(0.20f, 0.20f, 0.21f, 1.00f);
        Color popupSurface = Color(0.14f, 0.14f, 0.15f, 0.99f);

        Color border = Color(0.20f, 0.20f, 0.20f, 0.82f);
        Color borderFocus = Color(0.38f, 0.38f, 0.38f, 1.00f);

        Color text = Color(0.98f, 0.98f, 0.98f, 1.00f);
        Color textMuted = Color(0.48f, 0.48f, 0.48f, 1.00f);

        Color accent = Color(0.38f, 0.38f, 0.38f, 1.00f);
        Color accentStrong = Color(0.24f, 0.25f, 0.27f, 1.00f);
        Color itemHover = Color(0.20f, 0.21f, 0.23f, 1.00f);
        Color track = Color(0.12f, 0.12f, 0.13f, 0.96f);
        Color knob = Color(0.30f, 0.30f, 0.30f, 1.00f);
    };

    class ViewportTheme {
      public:
        static void cleanup();
        static SceneColors colors;
        static SchematicViewColors schematicViewColors;
        static SceneWidgetsColors sceneWidgetsColors;
        static void updateColorsFromImGuiStyle();

        static Color getCompHeaderColor(const std::string &group);

      private:
        static void initCompColorMap();
        static std::unordered_map<std::string, Color> &getCompHeaderColorMap();
    };
} // namespace Bess
