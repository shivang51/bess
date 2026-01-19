#pragma once

#include <string>
#include <vector>
namespace Bess::UI {
    class SettingsWindow {
      public:
        static void hide();
        static void show();
        static void draw();

        static bool isShown();

      private:
        static void onFirstDraw();

        static bool m_shown, m_isFirstDraw;
        static std::vector<float> m_availableScales;
        static std::vector<float> m_availableFontSizes;
        static std::vector<std::string> m_availableThemes;
        static std::vector<int> m_availableFps;
    };
} // namespace Bess::UI
