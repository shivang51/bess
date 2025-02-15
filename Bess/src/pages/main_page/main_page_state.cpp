#include "pages/main_page/main_page_state.h"
#include "common/types.h"

#include "components_manager/component_bank.h"
// #include "components_manager/components_manager.h"
#include "pages/main_page/main_page.h"

namespace Bess::Pages {

    std::shared_ptr<MainPageState> MainPageState::getInstance() {
        static std::shared_ptr<MainPageState> instance = std::make_shared<MainPageState>();
        return instance;
    }

    MainPageState::MainPageState() {
        createNewProject(false);
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

    void MainPageState::addFocusLostEvent(const uuids::uuid &uid) {
        // Simulator::Components::ComponentEventData e{};
        // e.type = Simulator::Components::ComponentEventType::focusLost;
        // Simulator::ComponentsManager::components[uid]->onEvent(e);
    }

    void MainPageState::addFocusEvent(const uuids::uuid &uid) {
        // Simulator::Components::ComponentEventData e{};
        // e.type = Simulator::Components::ComponentEventType::focus;
        // Simulator::ComponentsManager::components[uid]->onEvent(e);
    }

    void MainPageState::resetProjectState() {
        // m_drawMode = UI::Types::DrawMode::none;
        // m_bulkIds.clear();
        // m_prevBulkIds.clear();
        // m_connStartId = Simulator::ComponentsManager::emptyId;
        //
        // m_hoveredId = -1;
        // m_prevHoveredId = -1;
        // m_simulationPaused = false;
        //
        // Simulator::ComponentsManager::reset();
        // Simulator::Engine::clearQueue();
    }

    void MainPageState::createNewProject(bool updateWindowName) {
        resetProjectState();
        m_currentProjectFile = std::make_shared<ProjectFile>();
        if (!updateWindowName)
            return;
        auto win = MainPage::getTypedInstance()->getParentWindow();
        win->setName("Unnamed - BESS");
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
        auto win = MainPage::getTypedInstance()->getParentWindow();
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

    void MainPageState::setDragData(const Types::DragData &data) {
        m_dragData = data;
    }

    const Types::DragData &MainPageState::getDragData() {
        return m_dragData;
    }

    void MainPageState::clearDragData() {
        m_dragData = {};
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

    void MainPageState::setReadBulkIds(bool readBulkIds) {
        m_readBulkIds = readBulkIds;
    }

    bool MainPageState::shouldReadBulkIds() {
        return m_readBulkIds;
    }

    void MainPageState::addBulkId(const uuids::uuid &id) {
        m_bulkIds.emplace_back(id);
        addFocusEvent(id);
    }

    void MainPageState::setBulkIds(const std::vector<uuids::uuid> &ids) {
        clearBulkIds();
        m_bulkIds = ids;
        for (const auto &id : m_bulkIds) {
            addFocusEvent(id);
        }
    }

    void MainPageState::setBulkId(const uuids::uuid &id) {
        clearBulkIds();
        m_bulkIds.emplace_back(id);
        addFocusEvent(id);
    }

    const std::vector<uuids::uuid> &MainPageState::getBulkIds() {
        return m_bulkIds;
    }

    void MainPageState::clearBulkIds() {
        for (const auto &id : m_bulkIds) {
            addFocusLostEvent(id);
        }
        m_bulkIds.clear();
    }

    void MainPageState::removeBulkId(const uuids::uuid &id, bool dispatchEvent) {
        m_bulkIds.erase(std::remove(m_bulkIds.begin(), m_bulkIds.end(), id), m_bulkIds.end());
        if (dispatchEvent)
            addFocusLostEvent(id);
    }

    bool MainPageState::isBulkIdPresent(const uuids::uuid &id) {
        return std::ranges::find(m_bulkIds, id) != m_bulkIds.end();
    }

    bool MainPageState::isBulkIdEmpty() {
        return m_bulkIds.empty();
    }

    const uuids::uuid &MainPageState::getBulkIdAt(int index) {
        return m_bulkIds.at(index);
    }
} // namespace Bess::Pages
