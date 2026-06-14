#pragma once

#include "application/pages/main_page/services/hierarchical_scene_layout.h"
#include "application/project_file.h"
#include "bess_core/scene_driver.h"
#include "command_system.h"
#include "events/sim_engine_events.h"
#include "scene_events.h"
#include <vector>

namespace Bess {
    namespace Canvas {
        class Scene;
    }
    namespace SimEngine {
        class SimulationEngine;
    }
} // namespace Bess

namespace Bess::Pages {
    struct PageActionFlags {
        bool openProject = false;
        bool saveProject = false;
    };

    struct VerilogImportStatus {
        float progress = 0.f;
        std::string stageMessage;
        bool importing = false;
        bool finished = false;
        bool failed = false;
    };

    class MainPageState {
      public:
        MainPageState();
        ~MainPageState();

        Cmd::CommandSystem &getCommandSystem();

        void update();

        typedef std::unordered_map<UUID, std::string *> TNetIdToNameMap;
        MAKE_GETTER_SETTER(TNetIdToNameMap, NetIdToNameMap, m_netIdToNameMap)
        MAKE_GETTER_SETTER(std::unordered_set<UUID>, Probes, m_probes)

        typedef std::unordered_map<UUID, std::vector<UUID>> TNetIdToCompMap;
        TNetIdToCompMap &getNetIdToCompMap(UUID sceneId);

        void resetProjectState(bool updateWindowName = true);
        // creates default scenes in scene driver as well
        // and clears simulation engine and sets up new project file
        void createNewProject(bool updateWindowName = true);
        void saveCurrentProject() const;
        void loadProject(const std::string &path);
        bool importVerilogFile(const std::string &path,
                               std::string *errorMessage = nullptr);
        bool importVerilogFiles(const std::vector<std::string> &paths,
                                std::string *errorMessage = nullptr);
        HierarchicalSceneLayoutResult applyHierarchicalLayoutToActiveScene();
        void startVerilogImport(const std::string &path);
        void startVerilogImport(const std::vector<std::string> &paths);
        VerilogImportStatus
        advanceVerilogImport(std::string *errorMessage = nullptr);
        void cancelVerilogImport();

        void init();

        std::shared_ptr<SceneDriver> getSceneDriver() const;
        std::shared_ptr<SceneDriver> getSceneDriver();
        std::shared_ptr<ProjectFile> getCurrentProjectFile() const;

        PageActionFlags actionFlags = {};

        bool m_simulationPaused = false;
        void updateNets(const std::shared_ptr<Canvas::Scene> &scene);

        // contains the state of keyboard keys pressed

      private:
        void onEntityMoved(const Canvas::Events::EntityMovedEvent &e);
        void onEntityReparented(const Canvas::Events::EntityReparentedEvent &e);

        void onEntityAdded(const Canvas::Events::ComponentAddedEvent &e);
        void onEntityRemoved(const Canvas::Events::ComponentRemovedEvent &e);

        void onCompDefOutputsResized(
            const SimEngine::Events::CompDefOutputsResizedEvent &e);
        void onCompDefInputsResized(
            const SimEngine::Events::CompDefInputsResizedEvent &e);

      private:
        Cmd::CommandSystem m_commandSystem;
        std::unordered_map<int, bool> m_releasedKeysFrame;
        std::unordered_map<int, bool> m_pressedKeysFrame;
        std::unordered_map<int, bool> m_downKeys;
        struct SceneCompInfo {
            UUID sceneCompId;
            UUID sceneId;
        };
        std::unordered_map<UUID, SceneCompInfo> m_simIdToSceneCompId;
        std::unordered_set<UUID> m_probes;
        TNetIdToNameMap m_netIdToNameMap;
        std::unordered_map<UUID, TNetIdToCompMap> m_netIdToCompMap;
        struct VerilogImportSession;
        std::unique_ptr<VerilogImportSession> m_verilogImportSession;
    };
} // namespace Bess::Pages
