#include "proxy_slot_component.h"

namespace Bess::Canvas {
    void ProxySlotComponent::setInputOutputSlots(const UUID &inputSlotId,
                                                 const UUID &outputSlotId) {
        m_inputSlotId = inputSlotId;
        m_outputSlotId = outputSlotId;
    }

    void ProxySlotComponent::clearSlots() {
        m_inputSlotId = UUID::null;
        m_outputSlotId = UUID::null;
    }

    void ProxySlotComponent::clearConnections() {
        m_connections.clear();
    }

    void ProxySlotComponent::clear() {
        clearSlots();
        clearConnections();
    }

    void ProxySlotComponent::removeConnection(const UUID &connectionId) {
        m_connections.erase(std::ranges::remove(m_connections,
                                                connectionId)
                                .begin(),
                            m_connections.end());
    }

    void ProxySlotComponent::addConnection(const UUID &connectionId) {
        m_connections.push_back(connectionId);
    }
} // namespace Bess::Canvas
