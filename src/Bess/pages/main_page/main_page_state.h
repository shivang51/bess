#pragma once

#include "application/project_file.h"
#include "command_system.h"
#include "pages/main_page/scene_driver.h"

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

    class MainPageState {
      public:
        MainPageState() = default;

        Cmd::CommandSystem &getCommandSystem();

        void setKeyPressed(int key, bool pressed);
        bool isKeyPressed(int key);

        void resetProjectState() const;
        void createNewProject(bool updateWindowName = true);
        void saveCurrentProject() const;
        void loadProject(const std::string &path);
        void updateCurrentProject(const std::shared_ptr<ProjectFile> &project);

        void initCmdSystem(Canvas::Scene *scene,
                           SimEngine::SimulationEngine *simEngine);

        SceneDriver &getSceneDriver();
        std::shared_ptr<ProjectFile> getCurrentProjectFile() const;

        PageActionFlags actionFlags = {};

        // handle to currently open project file
        std::shared_ptr<ProjectFile> m_currentProjectFile;

        bool m_simulationPaused = false;

        // contains the state of keyboard keys pressed
        std::unordered_map<int, bool> m_pressedKeys;
        std::unordered_map<int, bool> releasedKeysFrame;
        std::unordered_map<int, bool> pressedKeysFrame;

      private:
        void onEntityMoved(const Canvas::Events::EntityMovedEvent &e);

      private:
        Cmd::CommandSystem m_commandSystem;
        SceneDriver m_sceneDriver;
    };
} // namespace Bess::Pages
