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
} // namespace Bess::Canvas
