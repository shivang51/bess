#include "components/slot.h"
#include "renderer/renderer.h"

#include "application_state.h"

#include "common/theme.h"
#include "ui.h"
#include <common/bind_helpers.h>

namespace Bess::Simulator::Components {

glm::vec3 connectedBg = {0.42f, 0.82f, 0.42f};

Slot::Slot(const UUIDv4::UUID &uid, int id, ComponentType type)
    : Component(uid, id, {0.f, 0.f, 0.f}, type) {
    m_events[ComponentEventType::leftClick] =
        (OnLeftClickCB)BIND_FN_1(Slot::onLeftClick);
    m_events[ComponentEventType::mouseHover] =
        (VoidCB)BIND_FN(Slot::onMouseHover);
}

void Slot::update(const glm::vec3 &pos) { m_position = pos; }

void Slot::render() {
    Renderer2D::Renderer::circle(m_position, 8.f,
                                 m_highlightBorder
                                     ? Theme::selectedWireColor
                                     : Theme::componentBorderColor,
                                 m_renderId);
    Renderer2D::Renderer::circle(
        m_position, m_highlightBorder ? 6.f : 7.f,
        (connections.size() == 0) ? Theme::backgroundColor : connectedBg,
        m_renderId);
}

void Slot::onLeftClick(const glm::vec2 &pos) {
    if (ApplicationState::drawMode == DrawMode::none) {
        ApplicationState::connStartId = m_uid;
        ApplicationState::points.emplace_back(m_position);
        ApplicationState::drawMode = DrawMode::connection;
        return;
    }

    auto slot = ComponentsManager::components[ApplicationState::connStartId];

    if (ApplicationState::connStartId == m_uid || slot->getType() == m_type)
        return;

    ComponentsManager::addConnection(ApplicationState::connStartId, m_uid);

    ApplicationState::drawMode = DrawMode::none;
    ApplicationState::connStartId = ComponentsManager::emptyId;
    ApplicationState::points.pop_back();
}

void Slot::onMouseHover() { UI::setCursorPointer(); }

void Slot::addConnection(const UUIDv4::UUID &uId) {
    if (isConnectedTo(uId)) return;
    connections.emplace_back(uId);
}

bool Slot::isConnectedTo(const UUIDv4::UUID& uId) {
    for (auto& conn : connections) {
        if (conn == uId) return true;
    }
    return false;
}

void Slot::highlightBorder(bool highlight) { m_highlightBorder = highlight; }
} // namespace Bess::Simulator::Components
