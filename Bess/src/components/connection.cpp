#include "components/connection.h"
#include "components/slot.h"
#include "ui/ui.h"

#include "common/helpers.h"
#include <imgui.h>

namespace Bess::Simulator::Components {
#define BIND_EVENT_FN_1(fn) std::bind(&fn, this, std::placeholders::_1)

#define BIND_EVENT_FN(fn) std::bind(&fn, this)

Connection::Connection(const uuids::uuid &uid, int renderId,
                       const uuids::uuid &slot1, const uuids::uuid &slot2)
    : Component(uid, renderId, {0.f, 0.f, -1.f}, ComponentType::connection) {
    m_slot1 = slot1;
    m_slot2 = slot2;
    m_events[ComponentEventType::leftClick] =
        (OnLeftClickCB)BIND_EVENT_FN_1(Connection::onLeftClick);
    m_events[ComponentEventType::focusLost] =
        (VoidCB)BIND_EVENT_FN(Connection::onFocusLost);
    m_events[ComponentEventType::focus] =
        (VoidCB)BIND_EVENT_FN(Connection::onFocus);
    m_events[ComponentEventType::mouseHover] = (VoidCB)BIND_EVENT_FN(Connection::onMouseHover);

    m_name = "Connection";
}

Connection::Connection(): Component()
{
}

void Connection::render() {
    auto slotA = ComponentsManager::components[m_slot1];
    auto slotB = ComponentsManager::components[m_slot2];
    auto m_selected = ApplicationState::getSelectedId() == m_uid;
    auto m_hovered = ApplicationState::hoveredId == m_renderId;

    auto slot = (Components::Slot*)slotA.get();

    auto posA = slotA->getPosition();
    auto posB = slotB->getPosition();
    Renderer2D::Renderer::curve(
        {posA.x, posA.y, m_position.z}, 
        {posB.x, posB.y, m_position.z},
        m_hovered ? 2.5f : 2.0f,
        m_selected ? Theme::selectedWireColor: (slot->getState() == DigitalState::high) ? Theme::stateHighColor : m_color,
        m_renderId
    );
}

void Connection::deleteComponent()
{
    auto slotA = (Slot*)ComponentsManager::components[m_slot1].get();
    slotA->removeConnection(m_slot2);
    auto slotB = (Slot*)ComponentsManager::components[m_slot2].get();
    slotB->removeConnection(m_slot1);

    if (slotA->getType() == ComponentType::inputSlot) {
        ComponentsManager::removeSlotsToConn(m_slot1, m_slot2);
    }
    else {
        ComponentsManager::removeSlotsToConn(m_slot2, m_slot1);
    }
}

void Connection::generate(const glm::vec3& pos){}

void Connection::drawProperties()
{
    ImGui::ColorEdit3("Wire Color", &m_color[0]);
}

void Connection::generate(const uuids::uuid& slot1, const uuids::uuid& slot2, const glm::vec3& pos)
{
    auto uid = Common::Helpers::uuidGenerator.getUUID();
    auto renderId = ComponentsManager::getNextRenderId();
    ComponentsManager::components[uid] = std::make_shared<Components::Connection>(uid, renderId, slot1, slot2);
    ComponentsManager::addRenderIdToCId(renderId, uid);
    ComponentsManager::addCompIdToRId(renderId, uid);
    ComponentsManager::renderComponenets.emplace_back(uid);
    ComponentsManager::addSlotsToConn(slot1, slot2, uid);
}

void Connection::onLeftClick(const glm::vec2 &pos) {
    ApplicationState::setSelectedId(m_uid);
}

void Connection::onMouseHover() {
    UI::UIMain::setCursorPointer();
}

void Connection::onFocusLost() {
    auto slotA = (Slot *)ComponentsManager::components[m_slot1].get();
    auto slotB = (Slot *)ComponentsManager::components[m_slot2].get();
    slotA->highlightBorder(false);
    slotB->highlightBorder(false);
}

void Connection::onFocus() {
    auto slotA = (Slot *)ComponentsManager::components[m_slot1].get();
    auto slotB = (Slot *)ComponentsManager::components[m_slot2].get();
    slotA->highlightBorder(true);
    slotB->highlightBorder(true);
}
} // namespace Bess::Simulator::Components
