#pragma once

#include "project_file.h"

namespace Bess::Pages {
    class MainPageState {
      public:
        static std::shared_ptr<MainPageState> getInstance();

        MainPageState();

        void setKeyPressed(int key, bool pressed);
        bool isKeyPressed(int key);

        void resetProjectState() const;
        void createNewProject(bool updateWindowName = true);
        void saveCurrentProject() const;
        void loadProject(const std::string &path);
        void updateCurrentProject(std::shared_ptr<ProjectFile> project);
        std::shared_ptr<ProjectFile> getCurrentProjectFile();

        // handle to currently open project file
        std::shared_ptr<ProjectFile> m_currentProjectFile;

        bool m_simulationPaused = false;

        // contains the state of keyboard keys pressed
        std::unordered_map<int, bool> m_pressedKeys = {};
    };
} // namespace Bess::Pages
