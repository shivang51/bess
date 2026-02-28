#pragma once

#include "common/bess_uuid.h"
#include "common/class_helpers.h"

namespace Bess::Canvas {
    class ProxySlotComponent {
      public:
        ProxySlotComponent() = default;

        virtual ~ProxySlotComponent() = default;

        MAKE_GETTER_SETTER(UUID, InputSlotId, m_inputSlotId)
        MAKE_GETTER_SETTER(UUID, OutputSlotId, m_outputSlotId)
        MAKE_GETTER_SETTER(std::vector<UUID>, Connections, m_connections)

        void setInputOutputSlots(const UUID &inputSlotId,
                                 const UUID &outputSlotId);

        void clearSlots();

        void clearConnections();

        void clear();

        void addConnection(const UUID &connectionId);

        void removeConnection(const UUID &connectionId);

      protected:
        UUID m_inputSlotId = UUID::null;
        UUID m_outputSlotId = UUID::null;
        std::vector<UUID> m_connections;
    };
} // namespace Bess::Canvas
