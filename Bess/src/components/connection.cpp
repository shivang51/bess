#include "components/connection.h"
#include "common/theme.h"
#include "components/slot.h"
#include "ui.h"

#include "common/helpers.h"

namespace Bess::Simulator::Components {
#define BIND_EVENT_FN_1(fn) std::bind(&fn, this, std::placeholders::_1)

#define BIND_EVENT_FN(fn) std::bind(&fn, this)

Connection::Connection(const UUIDv4::UUID &uid, int renderId,
                       const UUIDv4::UUID &slot1, const UUIDv4::UUID &slot2)
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

    auto posA = slotA->getPosition();
    auto posB = slotB->getPosition();
    Renderer2D::Renderer::curve(
        {posA.x, posA.y, m_position.z}, 
        {posB.x, posB.y, m_position.z},
        m_hovered ? 2.5f : 2.0f,
        m_selected ? Theme::selectedWireColor: Theme::wireColor,
        m_renderId
    );
}

void Connection::generate(const glm::vec3& pos){}

void Connection::generate(const UUIDv4::UUID& slot1, const UUIDv4::UUID& slot2, const glm::vec3& pos)
{
    auto uid = Common::Helpers::uuidGenerator.getUUID();
    auto renderId = ComponentsManager::getNextRenderId();
    ComponentsManager::components[uid] = std::make_shared<Components::Connection>(uid, renderId, slot1, slot2);
    ComponentsManager::addRenderIdToCId(renderId, uid);
    ComponentsManager::addCompIdToRId(renderId, uid);
    ComponentsManager::renderComponenets[uid] = ComponentsManager::components[uid];
}

void Connection::onLeftClick(const glm::vec2 &pos) {
    ApplicationState::setSelectedId(m_uid);
}

void Connection::onMouseHover() {
    UI::setCursorPointer();
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
