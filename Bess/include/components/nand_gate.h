#pragma once
#include "component.h"
#include "ext/vector_float2.hpp"
#include "components/slot.h"
#include <vector>

#include "uuid_v4.h"

namespace Bess::Simulator::Components {
    class NandGate : public Component {
    public:
        NandGate(const UUIDv4::UUID& uid, int renderId, glm::vec2 position, std::vector<UUIDv4::UUID> inputSlots, std::vector<UUIDv4::UUID> outputSlots);
        ~NandGate() = default;

        void render() override;

    private:
        std::vector<UUIDv4::UUID> m_inputSlots;
        std::vector<UUIDv4::UUID> m_outputSlots;


        void onLeftClick(const glm::vec2& pos);
        void onRightClick(const glm::vec2& pos);
    };
} // namespace Bess::Components