#include "components/nand_gate.h"

#include "fwd.hpp"
#include "renderer/renderer.h"
#include "ui.h"

#include "common/theme.h"

namespace Bess::Simulator::Components
{


    #define BIND_EVENT_FN_1(fn) \
    std::bind(&NandGate::fn, this, std::placeholders::_1)

    glm::vec2 gateSize = { 0.3f, 0.25f };

    NandGate::NandGate(const UUIDv4::UUID& uid, int renderId, glm::vec2 position, std::vector<UUIDv4::UUID> inputSlots, std::vector<UUIDv4::UUID> outputSlots) : Component(uid, renderId, position, ComponentType::gate) {
        m_inputSlots = inputSlots;
        m_outputSlots = outputSlots;

        m_events[ComponentEventType::leftClick] = (OnLeftClickCB)BIND_EVENT_FN_1(onLeftClick);
        m_events[ComponentEventType::rightClick] = (OnRightClickCB)BIND_EVENT_FN_1(onRightClick);
    }

    void NandGate::render()
    {
        bool selected = ApplicationState::getSelectedId() == m_uid;
        float thickness = selected ? 0.005f : 0.0035f;
        Renderer2D::Renderer::quad(
            { m_position.x - thickness, m_position.y + thickness },
            { gateSize.x + (thickness * 2.f), gateSize.y + (thickness * 2.f)},
            selected ? Theme::componentSelectionColor : Theme::componentBorderColor, m_renderId);

        Renderer2D::Renderer::quad(m_position, gateSize, Theme::componentBGColor, m_renderId);
        Renderer2D::Renderer::quad(m_position, { gateSize.x, 0.05f }, { .490f, .525f, 0.858f }, m_renderId);

        std::vector<glm::vec2> slots = {
            glm::vec2({0.025, -0.09f}),
            glm::vec2({0.025, -0.22f})
        };

        for (int i = 0; i < m_inputSlots.size(); i++) {
            Slot* slot = (Slot*)Simulator::ComponentsManager::components[m_inputSlots[i]].get();
            slot->update(m_position + slots[i]);
            slot->render();
        }

        for (int i = 0; i < m_outputSlots.size(); i++) {
            Slot* slot = (Slot*)Simulator::ComponentsManager::components[m_outputSlots[i]].get();
            slot->update(m_position + glm::vec2({ 0.3 - 0.025, -0.155f }));
            slot->render();
        }
    }

    void NandGate::onLeftClick(const glm::vec2& pos) {
        ApplicationState::setSelectedId(m_uid);
    }



	void NandGate::onRightClick(const glm::vec2& pos) {
        std::cout << "Right Click on nand gate" << std::endl;
    }
} // namespace Bess::Components
