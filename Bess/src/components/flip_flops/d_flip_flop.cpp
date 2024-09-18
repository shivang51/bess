#include "components/flip_flops/d_flip_flop.h"
#include "imgui.h"

namespace Bess::Simulator::Components {
    DFlipFlop::DFlipFlop(const uuids::uuid &uid, int renderId, glm::vec3 position, std::vector<uuids::uuid> inputSlots)
        : FlipFlop(uid, renderId, position, inputSlots) {
        m_name = name;
    }

    void DFlipFlop::update() {
        FlipFlop::update();
    }

    void DFlipFlop::generate(const glm::vec3 &pos) {
        FlipFlop::generate<DFlipFlop>(1, pos);
    }

    void DFlipFlop::drawProperties() {
    }

    void DFlipFlop::simulate() {}

} // namespace Bess::Simulator::Components
