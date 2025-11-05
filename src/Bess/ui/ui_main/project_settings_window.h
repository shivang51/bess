#pragma once
#include <memory>
#include <string>

#include "project_file.h"

namespace Bess::UI {
    class ProjectSettingsWindow {
      public:
        static void hide();
        static void show();
        static void draw();

        static bool isShown();

      private:
        static bool m_shown;
        static std::string m_projectName;
        static std::shared_ptr<ProjectFile> m_projectFile;
    };
} // namespace Bess::UI
