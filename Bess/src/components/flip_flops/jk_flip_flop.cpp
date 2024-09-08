#include "components/flip_flops/jk_flip_flop.h"

#include "common/digital_state.h"
#include "components/flip_flops/flip_flop.h"
#include "simulator/simulator_engine.h"

#include "components/slot.h"
#include "components_manager/components_manager.h"
#include "imgui.h"

namespace Bess::Simulator::Components {
    JKFlipFlop::JKFlipFlop(const uuids::uuid &uid, int renderId, glm::vec3 position, std::vector<uuids::uuid> inputSlots)
        : FlipFlop(uid, renderId, position, inputSlots) {
        m_name = "JK Flip Flop";
    }

    JKFlipFlop::JKFlipFlop(const uuids::uuid &uid, int renderId, glm::vec3 position, std::vector<uuids::uuid> inputSlots, std::vector<uuids::uuid> outputSlots, uuids::uuid clockSlot)
        : FlipFlop(uid, renderId, position, inputSlots, "JK Flip Flop", outputSlots, clockSlot) {
    }

    void JKFlipFlop::update() {
    }

    void JKFlipFlop::generate(const glm::vec3 &pos) {
        FlipFlop::generate<JKFlipFlop>(2, pos);
    }

    void JKFlipFlop::drawProperties() {
        ImGui::Text("JK Flip Flop");
    }

    void JKFlipFlop::simulate() {
        auto clkSlot = ComponentsManager::getComponent<Slot>(m_clockSlot);
        if (clkSlot->getState() == DigitalState::low) {
            Simulator::Engine::addToSimQueue(m_uid, m_clockSlot, DigitalState::low);
            return;
        }

        auto jslot = ComponentsManager::getComponent<Slot>(m_inputSlots[0]);
        auto kslot = ComponentsManager::getComponent<Slot>(m_inputSlots[1]);
        DigitalState j = jslot->getState();
        DigitalState k = kslot->getState();

        auto q = ComponentsManager::getComponent<Slot>(m_outputSlots[0]);
        auto q_ = ComponentsManager::getComponent<Slot>(m_outputSlots[1]);

        auto qState = q->getState();
        if (j == DigitalState::high && k == DigitalState::high) {
            qState = !qState;
        } else if (j == low && k == low) {
            q_->setState(m_uid, !qState);
        } else {
            qState = jslot->getState();
        }

        q->setState(m_uid, qState);
        q_->setState(m_uid, !qState);
    }

} // namespace Bess::Simulator::Components
