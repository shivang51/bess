#pragma once

#include "pages/page.h"
#include "application/types.h"
#include <memory>
#include <string>
#include <vector>

namespace Bess::Pages {
    class StartPage final : public Page {
      public:
        StartPage();
        static std::shared_ptr<Page> getInstance();

        void draw() override;
        void update(TFrameTime ts, const std::vector<ApplicationEvent> &events) override;

        struct PreviousProject {
            std::string name;
            std::string path;
        };

      private:
        static void drawTitle();
        std::vector<PreviousProject> m_previousProjects = {};
    };
} // namespace Bess::Pages
