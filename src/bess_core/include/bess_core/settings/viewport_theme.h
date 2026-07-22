#pragma once

#include "bess_core/renderer/renderer_types.h"
#include "bess_core/style/bess_theme.h"
#include "common/bess_api.h"
#include <string>

namespace Bess {
    using Color = Bess::Core::Renderer::Color;

    struct BESS_API SceneColors {
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

    struct BESS_API SchematicViewColors {
        Color pin = Color(0.20f, 0.75f, 0.85f, 1.0f);
        Color text = Color(0.85f, 0.85f, 0.85f, 1.0f);
        Color connection = Color(0.00f, 0.80f, 0.40f, 1.0f);
        Color componentFill = Color(0.08f, 0.09f, 0.11f, 1.0f);
        Color componentStroke = Color(0.45f, 0.50f, 0.60f, 1.0f);
        Color activeSignal = Color(1.00f, 0.90f, 0.20f, 1.0f);
    };

    struct BESS_API SceneWidgetsColors {
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

    struct BESS_API NodeHeaderColors {
        Color default_ = Color(0.45f, 0.45f, 0.45f, 0.90f);
        Color io = Color(0.48f, 0.35f, 0.58f, 0.90f);
        Color flipFlops = Color(0.48f, 0.35f, 0.58f, 0.90f);
        Color registersMemory = Color(0.48f, 0.35f, 0.58f, 0.90f);
        Color digitalGates = Color(0.32f, 0.56f, 0.32f, 0.90f);
        Color latches = Color(0.65f, 0.30f, 0.30f, 0.90f);
        Color combinationalCircuits = Color(0.25f, 0.55f, 0.55f, 0.90f);
    };

    class BESS_API ViewportTheme {
      public:
        static void cleanup();
        BESS_DATA_API static SceneColors colors;
        BESS_DATA_API static SchematicViewColors schematicViewColors;
        BESS_DATA_API static SceneWidgetsColors sceneWidgetsColors;
        BESS_DATA_API static NodeHeaderColors headerColors;
        static void updateColorsFromImGuiStyle(bool isDark);
        static void updateFromBessTheme(
            const std::shared_ptr<Core::Style::BessTheme> &theme);

        static Color getCompHeaderColor(const std::string &group);

        BESS_DATA_API static bool isDark;
    };
} // namespace Bess
