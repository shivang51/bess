#pragma once

#include "common/bess_api.h"
#include <memory>
#include <string>

#include "project_file.h"
#include "ui/ui_panel.h"

namespace Bess::UI {
    class BESS_API ProjectSettingsWindow : public Panel {
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
