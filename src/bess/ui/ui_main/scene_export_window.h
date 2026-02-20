#pragma once

namespace Bess::UI {
    class SceneExportWindow {
      public:
        static void show();
        static void draw();

        static bool isShown();

      private:
        static bool m_shown;
        static bool m_firstDraw;
    };
} // namespace Bess::UI
