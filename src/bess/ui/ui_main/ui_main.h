#pragma once

#include "fwd.hpp"
#include "pages/main_page/main_page_state.h"
#include "scene/camera.h"
#include "ui/ui_main/scene_viewport.h"

namespace Bess::UI {

    struct InternalData {
        std::string path;
        std::string statusMessage;
        bool newFileClicked = false, openFileClicked = false;
        bool exportSchematicClicked = false;
        bool isTbFocused = false;
    };

    struct UIState {
        SceneViewport mainViewport{"MainViewport"};
        InternalData _internalData;
    };

    class UIMain {
      public:
        static void draw();

        static UIState state;

        static void destroy();

      private:
        static void drawProjectExplorer();
        static void drawMenubar();
        static void drawStatusbar();
        static void drawViewport();
        static void resetDockspace();
        static void drawExternalWindows();
        static void onOpenProject();
        static void onSaveProject();

      private:
        // menu bar events
        static void onNewProject();

        static Pages::MainPageState *m_pageState;
    };
} // namespace Bess::UI
