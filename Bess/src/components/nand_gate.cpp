#include "components/nand_gate.h"

#include "fwd.hpp"
#include "renderer/renderer.h"
#include "ui.h"

#include "common/theme.h"

namespace Bess::Simulator::Components {

#define BIND_EVENT_FN_1(fn)                                                    \
    std::bind(&NandGate::fn, this, std::placeholders::_1)

glm::vec2 gateSize = {150.f, 100.f};

NandGate::NandGate(const UUIDv4::UUID &uid, int renderId, glm::vec3 position,
                   std::vector<UUIDv4::UUID> inputSlots,
                   std::vector<UUIDv4::UUID> outputSlots)
    : Component(uid, renderId, position, ComponentType::gate) {
    m_inputSlots = inputSlots;
    m_outputSlots = outputSlots;

    m_events[ComponentEventType::leftClick] =
        (OnLeftClickCB)BIND_EVENT_FN_1(onLeftClick);
    m_events[ComponentEventType::rightClick] =
        (OnRightClickCB)BIND_EVENT_FN_1(onRightClick);
}

void NandGate::render() {
    bool selected = ApplicationState::getSelectedId() == m_uid;
    float rPx = 24.f;
    float borderThickness = 2.f;

    float r = rPx / gateSize.y;
    float r1 = rPx / 20.f;

    Renderer2D::Renderer::quad(
        m_position, gateSize, Theme::componentBGColor, m_renderId, glm::vec4(r),
        selected ? glm::vec4(Theme::selectedCompColor, 1.f)
                 : Theme::componentBorderColor,
        borderThickness / gateSize.y);

    Renderer2D::Renderer::quad(
        {m_position.x,
         m_position.y + ((gateSize.y / 2) - 10.f) - borderThickness,
         m_position.z},
        {gateSize.x - borderThickness * 2, 20.f}, Theme::compHeaderColor,
        m_renderId, glm::vec4(r1, r1, 0.f, 0.f),
        glm::vec4(Theme::compHeaderColor, 1.f), borderThickness / 20.f);

    std::vector<glm::vec3> slots = {
        glm::vec3({-(gateSize.x / 2) + 10.f, 6.f, 0.f}),
        glm::vec3({-(gateSize.x / 2) + 10.f, -(gateSize.y / 2) + 16.0, 0.f})};

    for (int i = 0; i < m_inputSlots.size(); i++) {
        Slot *slot =
            (Slot *)Simulator::ComponentsManager::components[m_inputSlots[i]]
                .get();
        slot->update(m_position + slots[i]);
        slot->render();
    }

    for (int i = 0; i < m_outputSlots.size(); i++) {
        Slot *slot =
            (Slot *)Simulator::ComponentsManager::components[m_outputSlots[i]]
                .get();
        slot->update(m_position +
                     glm::vec3({gateSize.x / 2.f - 10.f, -10.f, 0.f}));
        slot->render();
    }
}

void NandGate::onLeftClick(const glm::vec2 &pos) {
    ApplicationState::setSelectedId(m_uid);
}

void NandGate::onRightClick(const glm::vec2 &pos) {
    std::cout << "Right Click on nand gate" << std::endl;
}
} // namespace Bess::Simulator::Components
