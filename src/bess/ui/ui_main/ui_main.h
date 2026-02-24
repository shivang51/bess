#pragma once

#include "common/bess_assert.h"
#include "fwd.hpp"
#include "pages/main_page/main_page_state.h"
#include "panel.h"
#include "ui/ui_main/scene_viewport.h"
#include "ui_panel.h"

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

    typedef std::function<void()> PreInitCallback;

    class UIMain {
      public:
        static void onPreInit(const PreInitCallback &callback);

        static void preInit();
        static void init();

        static void draw();

        template <typename TPanel>
        static void registerPanel() {
            BESS_ASSERT((std::is_base_of_v<Panel, TPanel>), "TPanel must be derived from Panel");
            getPanels().push_back(std::make_unique<TPanel>());
            BESS_DEBUG("Registered panel: {}", typeid(TPanel).name());
        }

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
        static std::vector<std::unique_ptr<Panel>> &getPanels();
        static std::vector<PreInitCallback> &getPreInitCallbacks();
        static Pages::MainPageState *m_pageState;
    };
} // namespace Bess::UI
