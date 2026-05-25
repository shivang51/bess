#pragma once

#include "common/class_helpers.h"
#include "common/sub_sys_container.h"
#include "common/sub_system.h"

#include <memory>
#include <string>

namespace Bess {
    class ProjectFile;
  namespace SimEngine {
    class SimulationEngine;
  }

    class BESS_API ProjectContext : public ISubSysContainer, public ISubSystem {
      public:
        ProjectContext() = default;

        void onInit() override;

        void onDestroy() override;

        void loadProject(const std::string &path);
        SimEngine::SimulationEngine &getSimEngine();

        MAKE_GETTER(std::shared_ptr<ProjectFile>, ProjectFile, m_projectFile)

      private:
        std::shared_ptr<ProjectFile> m_projectFile = nullptr;
    };

} // namespace Bess
