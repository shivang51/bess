#pragma once

#include "common/bess_assert.h"
#include "ui/ui_main/scene_viewport_panel.h"
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
        SceneViewportPanel mainViewport{"MainViewport"};
        InternalData _internalData{};
    };

    typedef std::function<void()> PreInitCallback;

    class UIMain {
      public:
        static void onPreInit(const PreInitCallback &callback);

        static void preInit();
        static void init();

        static void draw();
        static void update(TimeMs ts, const std::vector<ApplicationEvent> &events);

        template <typename TPanel, typename... Args>
        static void registerPanel(Args &&...args) {
            BESS_ASSERT((std::is_base_of_v<Panel, TPanel>), "TPanel must be derived from Panel");
            getPanels().push_back(std::make_shared<TPanel>(std::forward<Args>(args)...));
            const auto &panel = getPanels().back();
            getPanelMap()[typeid(TPanel)] = panel;
            if (std::is_same_v<TPanel, SceneViewportPanel>) {
                getScenePanels().push_back(
                    std::dynamic_pointer_cast<SceneViewportPanel>(panel));
            }
        }

        template <typename TPanel>
        static std::shared_ptr<TPanel> getPanel() {
            auto it = getPanelMap().find(typeid(TPanel));
            if (it != getPanelMap().end()) {
                return std::dynamic_pointer_cast<TPanel>(it->second);
            }
            return nullptr;
        }

        static UIState &getState();

        static void destroy();

        static std::vector<std::shared_ptr<SceneViewportPanel>> &getScenePanels();

      private:
        static void drawProjectExplorer();
        static void drawMenubar();
        static void drawStatusbar();
        static void resetDockspace();
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
