#include "application_state.h"
#include "components_manager/components_manager.h"

namespace Bess {
std::vector<glm::vec3> ApplicationState::points;
DrawMode ApplicationState::drawMode;

int ApplicationState::hoveredId;
int ApplicationState::prevHoveredId;

UUIDv4::UUID ApplicationState::m_selectedId;
UUIDv4::UUID ApplicationState::m_prevSelectedId;
 std::shared_ptr<ProjectFile> ApplicationState::currentProject = nullptr;


UUIDv4::UUID ApplicationState::connStartId;

DragData ApplicationState::dragData;

float ApplicationState::normalizingFactor;

void ApplicationState::init() {
    points = {};
    drawMode = DrawMode::none;

    hoveredId = -1;
    prevHoveredId = -1;

    m_selectedId = Simulator::ComponentsManager::emptyId;
    m_prevSelectedId = Simulator::ComponentsManager::emptyId;
    connStartId = Simulator::ComponentsManager::emptyId;

    dragData = {};
}

void ApplicationState::setSelectedId(const UUIDv4::UUID &uid) {
    m_prevSelectedId = m_selectedId;
    m_selectedId = uid;

    Simulator::Components::ComponentEventData e;
    if (m_prevSelectedId != Simulator::ComponentsManager::emptyId) {
        e.type = Simulator::Components::ComponentEventType::focusLost;
        Simulator::ComponentsManager::components[m_prevSelectedId]->onEvent(e);
    }
    if (m_selectedId != Simulator::ComponentsManager::emptyId) {
        e.type = Simulator::Components::ComponentEventType::focus;
        Simulator::ComponentsManager::components[m_selectedId]->onEvent(e);
    }
}

const UUIDv4::UUID &ApplicationState::getSelectedId() { return m_selectedId; }

const UUIDv4::UUID &ApplicationState::getPrevSelectedId() {
    return m_prevSelectedId;
}
} // namespace Bess
