#pragma once

#include "camera.h"
#include "fwd.hpp"
#include "pages/main_page/main_page_state.h"

namespace Bess::UI {

    struct InternalData {
        std::string path;
        bool newFileClicked = false, openFileClicked = false;
        bool isTbFocused = false;
    };

    struct UIState {
        glm::vec2 viewportSize = {800, 500};
        glm::vec2 viewportPos = {0, 0};
        std::uint64_t viewportTexture = 0;
        bool isViewportFocused = false;
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

        static std::shared_ptr<Pages::MainPageState> m_pageState;
    };
} // namespace Bess::UI
