#pragma once

namespace Bess::UI {
    class SettingsWindow {
    public:
        static void hide();
        static void show();
        static void draw();

        static bool isShown();
    private:
        static bool m_shown;
    };
}