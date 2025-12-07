#pragma once

#include "fwd.hpp"
#include "pages/main_page/main_page_state.h"
#include "scene/camera.h"
#include "ui/ui_main/scene_viewport.h"

namespace Bess::UI {

    struct InternalData {
        std::string path;
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

        static void setViewportTexture(std::uint64_t texture);

        static void drawStats(int fps);

        static UIState state;

      private:
        static void drawProjectExplorer();
        static void drawMenubar();
        static void drawStatusbar();
        static void drawViewport();
        static void resetDockspace();
        static void drawExternalWindows();

      private:
        // menu bar events
        static void onNewProject();
        static void onOpenProject();
        static void onSaveProject();
        static void onExportSceneView();

        static std::shared_ptr<Pages::MainPageState> m_pageState;
    };
} // namespace Bess::UI
