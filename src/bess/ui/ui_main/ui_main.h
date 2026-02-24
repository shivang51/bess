#pragma once

#include "common/bess_assert.h"
#include "ui/ui_main/scene_viewport.h"
#include "ui_panel.h"
#include <typeindex>

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
            getPanelMap()[typeid(TPanel)] = getPanels().back();
        }

        template <typename TPanel>
        static std::shared_ptr<TPanel> getPanel() {
            auto it = getPanelMap().find(typeid(TPanel));
            if (it != getPanelMap().end()) {
                return std::dynamic_pointer_cast<TPanel>(it->second);
            }
            return nullptr;
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
        static std::vector<std::shared_ptr<Panel>> &getPanels();
        static std::unordered_map<std::type_index, std::shared_ptr<Panel>> &getPanelMap();
        static std::vector<PreInitCallback> &getPreInitCallbacks();
    };
} // namespace Bess::UI
