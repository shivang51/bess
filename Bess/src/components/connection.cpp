#include "components/connection.h"
#include "ui.h"
#include "components/slot.h"
#include "common/theme.h"

namespace Bess::Simulator::Components {
    #define BIND_EVENT_FN_1(fn) \
    std::bind(&fn, this, std::placeholders::_1)

    #define BIND_EVENT_FN(fn) \
        std::bind(&fn, this)

    Connection::Connection(const UUIDv4::UUID& uid, int renderId, const UUIDv4::UUID&  slot1, const UUIDv4::UUID& slot2) : Component(uid, renderId, {0.f, 0.f}, ComponentType::connection) {
        m_slot1 = slot1;
        m_slot2 = slot2;
        m_events[ComponentEventType::leftClick] = (OnLeftClickCB)BIND_EVENT_FN_1(Connection::onLeftClick);
        m_events[ComponentEventType::focusLost] = (VoidCB)BIND_EVENT_FN(Connection::onFocusLost);
        m_events[ComponentEventType::focus] = (VoidCB)BIND_EVENT_FN(Connection::onFocus);
    }

    void Connection::render() {
        auto slotA = ComponentsManager::components[m_slot1];
        auto slotB = ComponentsManager::components[m_slot2];
        Renderer2D::Renderer::curve(slotA->getPosition(), slotB->getPosition(), ApplicationState::getSelectedId() == m_uid ? Theme::selectedWireColor : Theme::wireColor, m_renderId);
    }


    void Connection::onLeftClick(const glm::vec2& pos) {
        ApplicationState::setSelectedId(m_uid);
    }

    void Connection::onFocusLost(){
        auto slotA = (Slot*)ComponentsManager::components[m_slot1].get();
        auto slotB = (Slot*)ComponentsManager::components[m_slot2].get();
        slotA->highlightBorder(false);
        slotB->highlightBorder(false);
    }

    void Connection::onFocus(){
        auto slotA = (Slot*)ComponentsManager::components[m_slot1].get();
        auto slotB = (Slot*)ComponentsManager::components[m_slot2].get();
        slotA->highlightBorder(true);
        slotB->highlightBorder(true);
    }
}