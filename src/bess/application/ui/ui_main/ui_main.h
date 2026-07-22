#pragma once

#include "common/bess_api.h"

#include "common/bess_assert.h"
#include "ui/ui_main/scene_viewport_controller.h"
#include "ui/ui_main/scene_viewport_panel.h"
#include "ui/ui_panel.h"
#include <memory>
#include <typeindex>
#include <vector>

namespace Bess::UI {

    struct BESS_API InternalData {
        std::string path;
        std::string statusMessage;
        bool newFileClicked = false, openFileClicked = false;
        bool exportSchematicClicked = false;
        bool isTbFocused = false;
    };

    struct BESS_API UIState {
        SceneViewportPanel mainViewport{"MainViewport"};
        InternalData _internalData{};
    };

    typedef std::function<void()> PreInitCallback;

    class BESS_API UIMain {
      public:
        static void onPreInit(const PreInitCallback &callback);

        static void preInit();
        static void init();

        static void draw();
        static void update(TimeMs ts);

        template <typename TPanel, typename... Args>
        static void registerPanel(Args &&...args) {
            BESS_ASSERT((std::is_base_of_v<Panel, TPanel>),
                        "TPanel must be derived from Panel");
            getPanels().push_back(
                std::make_shared<TPanel>(std::forward<Args>(args)...));
            const auto &panel = getPanels().back();
            getPanelMap()[typeid(TPanel)] = panel;
            if (std::is_same_v<TPanel, SceneViewportPanel>) {
                getScenePanels().push_back(
                    std::dynamic_pointer_cast<SceneViewportPanel>(panel));
            }
        }

        template <typename TPanel> static std::shared_ptr<TPanel> getPanel() {
            auto it = getPanelMap().find(typeid(TPanel));
            if (it != getPanelMap().end()) {
                return std::dynamic_pointer_cast<TPanel>(it->second);
            }
            return nullptr;
        }

        static UIState &getState();

        static void destroy();

        static std::vector<std::shared_ptr<SceneViewportPanel>> &
        getScenePanels();

        static std::shared_ptr<SceneViewportPanel>
        getHoveredSceneViewportPanel();
        static std::shared_ptr<SceneViewportPanel>
        getFocusedSceneViewportPanel();
        static std::shared_ptr<SceneViewportPanel>
        getActiveSceneViewportPanel();
        static std::shared_ptr<SceneViewportPanel>
        getTargetSceneViewportPanel();
        static void setTargetSceneViewportPanel(
            const std::shared_ptr<SceneViewportPanel> &panel);

        static std::shared_ptr<Canvas::Scene> getHoveredViewportScene();
        static std::shared_ptr<Canvas::Scene> getFocusedViewportScene();
        static std::shared_ptr<Canvas::Scene> getActiveViewportScene();
        static std::shared_ptr<Canvas::Scene> getTargetViewportScene();
        static std::shared_ptr<Core::Viewport::ViewportContext>
        getActiveViewportContext();

        // Retained SceneView controllers (MainUIView). Preferred over the
        // legacy ImGui SceneViewportPanel path when both are present.
        static void registerSceneViewportController(
            const std::shared_ptr<SceneViewportController> &controller);
        static void
        unregisterSceneViewportController(const SceneViewportController *controller);
        static std::shared_ptr<SceneViewportController>
        getHoveredSceneViewportController();
        static std::shared_ptr<SceneViewportController>
        getFocusedSceneViewportController();
        static std::shared_ptr<SceneViewportController>
        getActiveSceneViewportController();

        static void regExtPanelDock(const std::string &panelName,
                                    const Dock &dock);

      private:
        static void drawProjectExplorer();
        static void drawMenubar();
        static void drawStatusbar();
        static void drawVerilogImportWizard();
        static void resetDockspace();
        static void refreshSceneViewportAttachments();
        static void updateSceneViewportTargets();
        static void clearSceneViewportTargets();
        static void onOpenProject();
        static void onSaveProject();

      private:
        static void onNewProject();
        static std::vector<std::shared_ptr<Panel>> &getPanels();
        static std::unordered_map<std::type_index, std::shared_ptr<Panel>> &
        getPanelMap();
        static std::vector<PreInitCallback> &getPreInitCallbacks();
        static std::unordered_map<std::string, Dock> &getExtPanelsDockMap();
        static std::vector<std::weak_ptr<SceneViewportController>> &
        getSceneViewportControllers();

        static bool m_isDockSpaceDirty;
        // When true, ImGui SceneViewportPanels stay hidden so the retained
        // SceneView owns interaction/render without double-driving the scene.
        static bool m_preferRetainedViewports;
    };
} // namespace Bess::UI
