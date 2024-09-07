#include "pages/main_page/main_page_state.h"
#include "common/types.h"

#include "components_manager/component_bank.h"
#include "components_manager/components_manager.h"
#include "pages/main_page/main_page.h"
#include "simulator/simulator_engine.h"

namespace Bess::Pages {

    std::shared_ptr<MainPageState> MainPageState::getInstance() {
        static std::shared_ptr<MainPageState> instance = std::make_shared<MainPageState>();
        return instance;
    }

    MainPageState::MainPageState() {
        resetProjectState();
    }

    int MainPageState::getHoveredId() {
        return m_hoveredId;
    }

    int MainPageState::getPrevHoveredId() {
        return m_prevHoveredId;
    }

    void MainPageState::setHoveredId(int id) {
        m_prevHoveredId = m_hoveredId;
        m_hoveredId = id;
    }

    bool MainPageState::isHoveredIdChanged() {
        return m_prevHoveredId != m_hoveredId;
    }

    void MainPageState::setMousePos(const glm::vec2 &pos) {
        m_mousePos = pos;
    }

    const glm::vec2 &MainPageState::getMousePos() {
        return m_mousePos;
    }

    void MainPageState::setSelectedId(const uuids::uuid &uid, bool updatePrevSel, bool dispatchFocusEvts) {
        uuids::uuid localPrev = m_selectedId;
        if (updatePrevSel)
            m_prevSelectedId = m_selectedId;
        m_selectedId = uid;

        if (!dispatchFocusEvts)
            return;

        Simulator::Components::ComponentEventData e{};
        if (localPrev != Simulator::ComponentsManager::emptyId) {
            e.type = Simulator::Components::ComponentEventType::focusLost;
            Simulator::ComponentsManager::components[localPrev]->onEvent(e);
        }
        if (m_selectedId != Simulator::ComponentsManager::emptyId) {
            e.type = Simulator::Components::ComponentEventType::focus;
            Simulator::ComponentsManager::components[m_selectedId]->onEvent(e);
        }
    }

    const uuids::uuid &MainPageState::getSelectedId() {
        return m_selectedId;
    }

    const uuids::uuid &MainPageState::getPrevSelectedId() {
        return m_prevSelectedId;
    }

    void MainPageState::resetProjectState() {
        m_drawMode = UI::Types::DrawMode::none;
        m_selectedId = Simulator::ComponentsManager::emptyId;
        m_prevSelectedId = Simulator::ComponentsManager::emptyId;
        m_connStartId = Simulator::ComponentsManager::emptyId;

        m_hoveredId = -1;
        m_prevHoveredId = -1;
        m_simulationPaused = false;

        Simulator::ComponentsManager::reset();
        Simulator::Engine::clearQueue();
    }

    void MainPageState::createNewProject() {
        resetProjectState();
        updateCurrentProject(std::make_shared<ProjectFile>());
    }

    void MainPageState::loadProject(const std::string &path) {
        resetProjectState();
        auto project = std::make_shared<ProjectFile>(path);
        updateCurrentProject(project);
    }

    void MainPageState::saveCurrentProject() {
        m_currentProjectFile->save();
    }

    void MainPageState::updateCurrentProject(std::shared_ptr<ProjectFile> project) {
        if (project == nullptr)
            return;
        m_currentProjectFile = project;
        MainPage::getTypedInstance()->getParentWindow()->setName(m_currentProjectFile->getName() + " - BESS");
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

    void MainPageState::setConnStartId(const uuids::uuid &uid) {
        m_connStartId = uid;
    }

    const uuids::uuid &MainPageState::getConnStartId() {
        return m_connStartId;
    }

    void MainPageState::setSimulationPaused(bool paused) {
        m_simulationPaused = paused;
    }

    bool MainPageState::isSimulationPaused() {
        return m_simulationPaused;
    }

    void MainPageState::setDrawMode(UI::Types::DrawMode mode) {
        m_drawMode = mode;
    }

    UI::Types::DrawMode MainPageState::getDrawMode() {
        return m_drawMode;
    }

    void MainPageState::setDragData(const UI::Types::DragData &data) {
        m_dragData = data;
    }

    const UI::Types::DragData &MainPageState::getDragData() {
        return m_dragData;
    }

    UI::Types::DragData &MainPageState::getDragDataRef() {
        return m_dragData;
    }

    void MainPageState::setPoints(const std::vector<glm::vec3> &points) {
        m_points = points;
    }

    const std::vector<glm::vec3> &MainPageState::getPoints() {
        return m_points;
    }

    void MainPageState::addPoint(const glm::vec3 &point) {
        m_points.push_back(point);
    }

    void MainPageState::clearPoints() {
        m_points.clear();
    }

    std::vector<glm::vec3> &MainPageState::getPointsRef() {
        return m_points;
    }

    void MainPageState::setPrevGenBankElement(const Simulator::ComponentBankElement &element) {
        m_prevGenBankElement = (Simulator::ComponentBankElement *)&element;
    }

    Simulator::ComponentBankElement *MainPageState::getPrevGenBankElement() {
        return m_prevGenBankElement;
    }

} // namespace Bess::Pages
