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

        void setInputOutputSlots(const UUID &inputSlotId,
                                 const UUID &outputSlotId);

        void clearSlots();

      protected:
        UUID m_inputSlotId = UUID::null;
        UUID m_outputSlotId = UUID::null;
    };
} // namespace Bess::Canvas
