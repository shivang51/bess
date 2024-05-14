#include "components/nand_gate.h"

#include "fwd.hpp"
#include "renderer/renderer.h"
#include "ui.h"

#include "common/theme.h"

namespace Bess::Simulator::Components
{

#define BIND_EVENT_FN_1(fn) \
    std::bind(&NandGate::fn, this, std::placeholders::_1)

    glm::vec2 gateSize = {120.f, 80.f};

    NandGate::NandGate(const UUIDv4::UUID &uid, int renderId, glm::vec2 position, std::vector<UUIDv4::UUID> inputSlots, std::vector<UUIDv4::UUID> outputSlots) : Component(uid, renderId, position, ComponentType::gate)
    {
        m_inputSlots = inputSlots;
        m_outputSlots = outputSlots;

        m_events[ComponentEventType::leftClick] = (OnLeftClickCB)BIND_EVENT_FN_1(onLeftClick);
        m_events[ComponentEventType::rightClick] = (OnRightClickCB)BIND_EVENT_FN_1(onRightClick);
    }

    void NandGate::render()
    {
        bool selected = ApplicationState::getSelectedId() == m_uid;
        float thickness = 1.f;
        float r = 12.f;

        glm::vec2 borderBoxSize = gateSize + (thickness * 2);
        Renderer2D::Renderer::quad(
            {m_position.x, m_position.y},
            borderBoxSize,
            selected ? Theme::selectedCompColor : Theme::componentBorderColor,
            m_renderId,
            glm::vec4(r * (1.f / borderBoxSize.y)));

        Renderer2D::Renderer::quad(m_position, gateSize, Theme::componentBGColor, m_renderId, glm::vec4(r * (1.f / gateSize.y)));
        Renderer2D::Renderer::quad({m_position.x, m_position.y + ((gateSize.y / 2) - 10.f)}, {gateSize.x, 20.f}, Theme::compHeaderColor, m_renderId, glm::vec4(r / 20.f, r / 20.f, 0.f, 0.f));

        std::vector<glm::vec2> slots = {
            glm::vec2({-(gateSize.x / 2) + 10.f, 6.f}),
            glm::vec2({-(gateSize.x / 2) + 10.f, -(gateSize.y / 2) + 16.0})};

        for (int i = 0; i < m_inputSlots.size(); i++)
        {
            Slot *slot = (Slot *)Simulator::ComponentsManager::components[m_inputSlots[i]].get();
            slot->update(m_position + slots[i]);
            slot->render();
        }

        for (int i = 0; i < m_outputSlots.size(); i++)
        {
            Slot *slot = (Slot *)Simulator::ComponentsManager::components[m_outputSlots[i]].get();
            slot->update(m_position + glm::vec2({gateSize.x / 2.f - 10.f, -10.f}));
            slot->render();
        }
    }

    void NandGate::onLeftClick(const glm::vec2 &pos)
    {
        ApplicationState::setSelectedId(m_uid);
    }

    void NandGate::onRightClick(const glm::vec2 &pos)
    {
        std::cout << "Right Click on nand gate" << std::endl;
    }
} // namespace Bess::Components
