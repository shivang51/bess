#pragma once

#include "application/pages/main_page/scene_driver.h"
#include "application/project_file.h"
#include "command_system.h"
#include "events/sim_engine_events.h"
#include "scene.h"
#include "scene_events.h"

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

        void setKeyPressed(int key);

        // returns true if the key was pressed in the current frame, false otherwise
        bool isKeyPressed(int key) const;

        void setKeyReleased(int key);

        // returns true if the key was released in the current frame, false otherwise
        bool isKeyReleased(int key) const;

        void setKeyDown(int key, bool isDown);

        // returns true if the key is currently being held down, false otherwise
        bool isKeyDown(int key) const;

        typedef std::unordered_map<UUID, std::string *> TNetIdToNameMap;
        MAKE_GETTER_SETTER(TNetIdToNameMap, NetIdToNameMap, m_netIdToNameMap)
        MAKE_GETTER_SETTER(std::unordered_set<UUID>, Probes, m_probes)

        typedef std::unordered_map<UUID, std::vector<UUID>> TNetIdToCompMap;
        TNetIdToCompMap &getNetIdToCompMap(UUID sceneId);

        void resetProjectState() const;
        void createNewProject(bool updateWindowName = true);
        void saveCurrentProject() const;
        void loadProject(const std::string &path);
        void updateCurrentProject(const std::shared_ptr<ProjectFile> &project);
        bool importVerilogFile(const std::string &path, std::string *errorMessage = nullptr);
        void startVerilogImport(const std::string &path);
        VerilogImportStatus advanceVerilogImport(std::string *errorMessage = nullptr);
        void cancelVerilogImport();

        void initCmdSystem();

        const SceneDriver &getSceneDriver() const;
        SceneDriver &getSceneDriver();
        std::shared_ptr<ProjectFile> getCurrentProjectFile() const;

        PageActionFlags actionFlags = {};

        // handle to currently open project file
        std::shared_ptr<ProjectFile> m_currentProjectFile;

        bool m_simulationPaused = false;

        // contains the state of keyboard keys pressed

      private:
        void onEntityMoved(const Canvas::Events::EntityMovedEvent &e);
        void onEntityReparented(const Canvas::Events::EntityReparentedEvent &e);

        void onEntityAdded(const Canvas::Events::ComponentAddedEvent &e);
        void onEntityRemoved(const Canvas::Events::ComponentRemovedEvent &e);

        void onCompDefOutputsResized(const SimEngine::Events::CompDefOutputsResizedEvent &e);
        void onCompDefInputsResized(const SimEngine::Events::CompDefInputsResizedEvent &e);

      private:
        Cmd::CommandSystem m_commandSystem;
        SceneDriver m_sceneDriver;
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
