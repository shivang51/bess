#pragma once

#include "application/project_file.h"

namespace Bess::Pages {
    struct PageActionFlags {
        bool openProject = false;
        bool saveProject = false;
    };

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

        PageActionFlags actionFlags = {};

        // handle to currently open project file
        std::shared_ptr<ProjectFile> m_currentProjectFile;

        bool m_simulationPaused = false;

        // contains the state of keyboard keys pressed
        std::unordered_map<int, bool> m_pressedKeys;
        std::unordered_map<int, bool> releasedKeysFrame;
    };
} // namespace Bess::Pages
