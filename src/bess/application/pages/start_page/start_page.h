#pragma once

#include "common/bess_api.h"

#include "common/types.h"
#include "pages/page.h"
#include <memory>
#include <string>
#include <vector>

namespace Bess::Pages {
    class BESS_API StartPage final : public Page {
      public:
        StartPage() = default;
        static std::shared_ptr<Page> getInstance();

        void draw() override;
        void update(TimeMs ts) override;

        struct PreviousProject {
            std::string name;
            std::string path;
        };

      private:
        static void drawTitle();
        std::vector<PreviousProject> m_previousProjects = {};
    };
} // namespace Bess::Pages
