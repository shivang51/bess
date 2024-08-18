#include "application_state.h"
#include "components_manager/components_manager.h"
#include "simulator/simulator_engine.h"

namespace Bess {
std::vector<glm::vec3> ApplicationState::points;
DrawMode ApplicationState::drawMode;

int ApplicationState::hoveredId;
int ApplicationState::prevHoveredId;

uuids::uuid ApplicationState::m_selectedId;
uuids::uuid ApplicationState::m_prevSelectedId;
 std::shared_ptr<ProjectFile> ApplicationState::currentProject = nullptr;

 Window* ApplicationState::m_mainWindow = nullptr;

uuids::uuid ApplicationState::connStartId;

DragData ApplicationState::dragData;

float ApplicationState::normalizingFactor;

 bool ApplicationState::simulationPaused = false;


void ApplicationState::init(Window* mainWin) {
    m_mainWindow = mainWin;

    points = {};
    drawMode = DrawMode::none;

    hoveredId = -1;
    prevHoveredId = -1;

    m_selectedId = Simulator::ComponentsManager::emptyId;
    m_prevSelectedId = Simulator::ComponentsManager::emptyId;
    connStartId = Simulator::ComponentsManager::emptyId;

    dragData = {};
    updateCurrentProject(std::make_shared<ProjectFile>());
}

void ApplicationState::setSelectedId(const uuids::uuid &uid, bool updatePrevSel, bool dispatchFocusEvts) {
    uuids::uuid localPrev = m_selectedId;
    if(updatePrevSel) m_prevSelectedId = m_selectedId;
    m_selectedId = uid;

    if (!dispatchFocusEvts) return;

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

const uuids::uuid &ApplicationState::getSelectedId() { return m_selectedId; }

const uuids::uuid &ApplicationState::getPrevSelectedId() {
    return m_prevSelectedId;
}

void ApplicationState::createNewProject()
{
    init(m_mainWindow);
    Simulator::ComponentsManager::reset();
    Simulator::Engine::clearQueue();
}

void ApplicationState::saveCurrentProject()
{
    currentProject->save();
}

void ApplicationState::loadProject(const std::string& path)
{
    init(m_mainWindow);
    Simulator::ComponentsManager::reset();
    Simulator::Engine::clearQueue();

    auto project = std::make_shared<ProjectFile>(path);
    updateCurrentProject(project);
    //Simulator::Engine::RefreshSimulation();
}

void ApplicationState::updateCurrentProject(std::shared_ptr<ProjectFile> project)
{
    if (project == nullptr) return;
    currentProject = project;
    m_mainWindow->setName(currentProject->getName() + " - BESS");
}
} // namespace Bess
