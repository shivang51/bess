#pragma once
#include <memory>
#include <string>

#include "application/project_file.h"
#include "ui_panel.h"

namespace Bess::UI {
    class ProjectSettingsWindow : public Panel {
      public:
        ProjectSettingsWindow();

        void onBeforeDraw() override;
        void onDraw() override;

        void onShow() override;

      private:
        std::string m_projectName;
        std::shared_ptr<ProjectFile> m_projectFile;
    };
} // namespace Bess::UI
