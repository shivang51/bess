#include "pages/main_page/main_page_state.h"
#include "pages/main_page/cmds/update_vec_cmd.h"
#include "pages/main_page/main_page.h"
#include "simulation_engine.h"

namespace Bess::Pages {

    void MainPageState::resetProjectState() const {
        m_sceneDriver.getActiveScene()->clear();
        SimEngine::SimulationEngine::instance().clear();
    }

    void MainPageState::createNewProject(bool updateWindowName) {
        resetProjectState();
        m_currentProjectFile = std::make_shared<ProjectFile>();
        if (!updateWindowName)
            return;
        const auto win = MainPage::getTypedInstance()->getParentWindow();
        win->setName(m_currentProjectFile->getName());
    }

    void MainPageState::loadProject(const std::string &path) {
        resetProjectState();
        const auto project = std::make_shared<ProjectFile>(path);
        updateCurrentProject(project);
    }

    void MainPageState::saveCurrentProject() const {
        m_currentProjectFile->save();
    }

    void MainPageState::updateCurrentProject(const std::shared_ptr<ProjectFile> &project) {
        if (project == nullptr)
            return;
        m_currentProjectFile = project;
        const auto win = MainPage::getTypedInstance()->getParentWindow();
        win->setName(m_currentProjectFile->getName() + " - BESS");
    }

    std::shared_ptr<ProjectFile> MainPageState::getCurrentProjectFile() const {
        return m_currentProjectFile;
    }

    bool MainPageState::isKeyPressed(int key) {
        return m_pressedKeys[key];
    }

    void MainPageState::setKeyPressed(int key, bool pressed) {
        m_pressedKeys[key] = pressed;
    }

    void MainPageState::initCmdSystem(Canvas::Scene *scene,
                                      SimEngine::SimulationEngine *simEngine) {

        m_commandSystem.init(scene, simEngine);
        EventSystem::EventDispatcher::instance().sink<Canvas::Events::EntityMovedEvent>().connect<&MainPageState::onEntityMoved>(this);
    }

    void MainPageState::onEntityMoved(const Canvas::Events::EntityMovedEvent &e) {
        auto entity = m_sceneDriver->getState().getComponentByUuid(e.entityUuid);
        glm::vec3 *posPtr = &entity->getTransform().position;

        if (entity) {
            // auto cmd = std::make_unique<Cmd::UpdateVecCommand<glm::vec3>>(posPtr, e.newPos, e.oldPos);
            // m_commandSystem.push(std::move(cmd));
        }
    }

    SceneDriver &MainPageState::getSceneDriver() {
        return m_sceneDriver;
    }
} // namespace Bess::Pages
