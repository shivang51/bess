#pragma once

#include "camera.h"
#include "fwd.hpp"
#include "glad/glad.h"
#include "pages/main_page/main_page_state.h"
#include <GLFW/glfw3.h>

namespace Bess::UI {

    struct InternalData {
        std::string path;
        bool newFileClicked = false, openFileClicked = false;
    };

    struct UIState {
        float cameraZoom = Camera::defaultZoom;
        glm::vec2 viewportSize = {800, 500};
        glm::vec2 viewportPos = {0, 0};
        GLuint64 viewportTexture = 0;

        InternalData _internalData;
    };

    class UIMain {
      public:
        static void draw();

        static void setViewportTexture(GLuint64 texture);

        static void drawStats(int fps);

        static UIState state;

      private:
        static void drawProjectExplorer();
        static void drawMenubar();
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
