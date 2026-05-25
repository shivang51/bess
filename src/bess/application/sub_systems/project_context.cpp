#include "project_context.h"
#include "pages/main_page/scene_driver.h"
#include "simulation_engine.h"

namespace Bess {
    void ProjectContext::onInit() {
        addSubSystem<SceneDriver>();
        addSubSystem<SimEngine::SimulationEngine>();

        ISubSysContainer::init();
    }

    void ProjectContext::onDestroy() { ISubSysContainer::destroy(); }

    void ProjectContext::loadProject(const std::string &path) {
        if (path.empty()) {
            BESS_ERROR("Project path is empty. Cannot load project.");
            return;
        }

        auto sceneDriver = getSubSystem<SceneDriver>();

        BESS_ASSERT(sceneDriver, "SceneDriver subsystem must be initialized "
                                 "before loading a project");

        sceneDriver->reset(true);

        auto simEngine = getSubSystem<SimEngine::SimulationEngine>();
        BESS_ASSERT(simEngine, "SimulationEngine subsystem must be initialized "
                               "before loading a project");
        simEngine->clear();

        m_projectFile = std::make_shared<ProjectFile>(path);
    }

} // namespace Bess
