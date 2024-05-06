#pragma once
#include "component.h"
#include "slot.h"

namespace Bess::Simulator::Components {
	class OutputProbe : public Component {
    public:
        OutputProbe(const UUIDv4::UUID& uid, int renderId, glm::vec2 position,const UUIDv4::UUID& outputSlot );
        ~OutputProbe() = default;

        void render() override;

    private:
        UUIDv4::UUID m_outputSlot;
	};
}
