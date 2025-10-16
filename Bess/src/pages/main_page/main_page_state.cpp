#include "pages/main_page/main_page_state.h"

#include "pages/main_page/main_page.h"

namespace Bess::Pages {

    std::shared_ptr<MainPageState> MainPageState::getInstance() {
        static std::shared_ptr<MainPageState> instance = std::make_shared<MainPageState>();
        return instance;
    }

    MainPageState::MainPageState() {
        createNewProject(false);
    }

    void MainPageState::resetProjectState() const {
        Canvas::Scene::instance()->clear();
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

    void MainPageState::updateCurrentProject(std::shared_ptr<ProjectFile> project) {
        if (project == nullptr)
            return;
        m_currentProjectFile = project;
        const auto win = MainPage::getTypedInstance()->getParentWindow();
        win->setName(m_currentProjectFile->getName() + " - BESS");
    }

    std::shared_ptr<ProjectFile> MainPageState::getCurrentProjectFile() {
        return m_currentProjectFile;
    }

    bool MainPageState::isKeyPressed(int key) {
        return m_pressedKeys[key];
    }

    void MainPageState::setKeyPressed(int key, bool pressed) {
        m_pressedKeys[key] = pressed;
    }
} // namespace Bess::Pages
