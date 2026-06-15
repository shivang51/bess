#include "bess_core/project_context.h"

#include "bess_core/connection_service.h"
#include "bess_core/copy_paste_service.h"
#include "bess_core/scene_driver.h"
#include "command_system.h"
#include "common/logger.h"
#include "common/sub_sys_container.h"
#include "project_file.h"
#include "simulation_engine.h"
// #include "window.h"

namespace Bess {
    void ProjectContext::onBeginFrame() {
        ISubSysContainer::beginFrame();
    }

    void ProjectContext::onEndFrame() {
        ISubSysContainer::endFrame();
    }

    void ProjectContext::onUpdate(TimeMs ts) {
        ISubSysContainer::update(ts);
    }

    void ProjectContext::onInit() {
        addSubSystem<SceneDriver>();
        addSubSystem<SimEngine::SimulationEngine>();
        addSubSystem<Svc::SvcConnection>();
        addSubSystem<Svc::CopyPaste::Context>();
        addSubSystem<Cmd::CommandSystem>();

        ISubSysContainer::init();
    }

    void ProjectContext::onDestroy() {
        ISubSysContainer::destroy();
    }

    SimEngine::SimulationEngine &ProjectContext::getSimEngine() {
        auto simEngine = getSubSystem<SimEngine::SimulationEngine>();
        BESS_ASSERT(simEngine,
                    "SimulationEngine subsystem must be initialized "
                    "before accessing ProjectContext");
        return *simEngine;
    }

    void ProjectContext::loadProject(const std::string &path) {
        if (path.empty()) {
            BESS_ERROR("Project path is empty. Cannot load project.");
            return;
        }

        auto sceneDriver = getSubSystem<SceneDriver>();

        BESS_ASSERT(sceneDriver,
                    "SceneDriver subsystem must be initialized "
                    "before loading a project");

        sceneDriver->reset();

        auto simEngine = getSubSystem<SimEngine::SimulationEngine>();
        BESS_ASSERT(simEngine,
                    "SimulationEngine subsystem must be initialized "
                    "before loading a project");
        simEngine->clear();

        m_projectFile = std::make_shared<ProjectFile>(path);

        // auto &appCtx = GAppContext::getInstance();
        // const auto &win = appCtx.getSubSystem<Bess::Window>();
        // win->setName(m_projectFile->getName() + " - BESS");
    }

    void ProjectContext::saveProject() const {
        if (m_projectFile) {
            m_projectFile->save();
        } else {
            BESS_WARN("No project file loaded. Cannot save project.");
        }
    }

    void ProjectContext::createNewProject() {
        auto sceneDriver = getSubSystem<SceneDriver>();
        BESS_ASSERT(sceneDriver,
                    "SceneDriver subsystem must be initialized "
                    "before creating a new project");
        sceneDriver->reset();

        auto simEngine = getSubSystem<SimEngine::SimulationEngine>();
        BESS_ASSERT(simEngine,
                    "SimulationEngine subsystem must be initialized "
                    "before creating a new project");
        simEngine->clear();

        m_projectFile = std::make_shared<ProjectFile>();
        auto &appCtx = GAppContext::getInstance();
        // const auto &win = appCtx.getSubSystem<Bess::Window>();
        // win->setName(m_projectFile->getName() + " - BESS");
        // win->setName("Untitled Project - BESS");

        BESS_DEBUG("Created new project");
    }

} // namespace Bess
