#include "components/nand_gate.h"

#include "application_state.h"
#include "fwd.hpp"
#include "renderer/renderer.h"

#include "common/theme.h"
#include "common/helpers.h"

namespace Bess::Simulator::Components {

#define BIND_EVENT_FN_1(fn)                                                    \
    std::bind(&NandGate::fn, this, std::placeholders::_1)

glm::vec2 gateSize = {150.f, 100.f};

NandGate::NandGate() : Component()
{
}

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
    float borderThickness = 3.f;

    float r = rPx / gateSize.y;
    float r1 = rPx / 20.f;

    Renderer2D::Renderer::quad(
        m_position, gateSize, Theme::componentBGColor, m_renderId, glm::vec4(r),
        selected ? glm::vec4(Theme::selectedCompColor, 1.f)
                 : Theme::componentBorderColor,
        borderThickness / gateSize.y);

    Renderer2D::Renderer::quad(
        {m_position.x,
         m_position.y + ((gateSize.y / 2) - 10.f) - borderThickness / 2.f,
         m_position.z},
        {gateSize.x - borderThickness, 20.f}, Theme::compHeaderColor,
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
        
        Renderer2D::Renderer::text(std::string(1, 'A' + i), m_position + slots[i] + glm::vec3({10.f, -2.5f, ComponentsManager::zIncrement}), 12.f, {1.f, 1.f, 1.f}, m_renderId);

    }

    for (int i = 0; i < m_outputSlots.size(); i++) {
        Slot *slot =
            (Slot *)Simulator::ComponentsManager::components[m_outputSlots[i]]
                .get();
        slot->update(m_position +
                     glm::vec3({gateSize.x / 2.f - 10.f, -10.f, 0.f}));
        slot->render();
    }


    Renderer2D::Renderer::text("Nand Gate", Common::Helpers::GetLeftCornerPos(m_position, gateSize) + glm::vec3({ 8.f, -16.f, ComponentsManager::zIncrement }), 12.f, {1.f, 1.f, 1.f}, m_renderId);
}

void NandGate::simulate()
{
    int output = 1;

    for (auto& slotId : m_inputSlots) {
        auto slot = (Slot*)ComponentsManager::components[slotId].get();
        output *= static_cast<std::underlying_type_t<DigitalState>>(slot->getState());
    }

    DigitalState outputState = static_cast<DigitalState>(!output);

    for (auto& slotId : m_outputSlots) {
        auto slot = (Slot*)ComponentsManager::components[slotId].get();
        slot->setState(m_uid, outputState);
    }
}

void NandGate::generate(const glm::vec3& pos)
{
    auto pId = Common::Helpers::uuidGenerator.getUUID();
    // input slots
    int n = 2;
    std::vector<UUIDv4::UUID> inputSlots;
    while (n--) {
        auto uid = Common::Helpers::uuidGenerator.getUUID();
        auto renderId = ComponentsManager::getNextRenderId();
        ComponentsManager::components[uid] = std::make_shared<Components::Slot>(
            uid, pId, renderId, ComponentType::inputSlot);
        ComponentsManager::addRenderIdToCId(renderId, uid);
        ComponentsManager::addCompIdToRId(renderId, uid);
        inputSlots.emplace_back(uid);
    }

    // output slots
    std::vector<UUIDv4::UUID> outputSlots;
    n = 1;
    while (n--) {
        auto uid = Common::Helpers::uuidGenerator.getUUID();
        auto renderId = ComponentsManager::getNextRenderId();
        ComponentsManager::components[uid] = std::make_shared<Components::Slot>(
            uid, pId, renderId, ComponentType::outputSlot);
        ComponentsManager::addRenderIdToCId(renderId, uid);
        ComponentsManager::addCompIdToRId(renderId, uid);
        outputSlots.emplace_back(uid);
    }

    auto& uid = pId;
    auto renderId = ComponentsManager::getNextRenderId();

    auto pos_ = pos;
    pos_.z = ComponentsManager::getNextZPos();

    ComponentsManager::components[uid] = std::make_shared<Components::NandGate>(
        uid, renderId, pos_, inputSlots, outputSlots);

    ComponentsManager::addRenderIdToCId(renderId, uid);
    ComponentsManager::addCompIdToRId(renderId, uid);

    ComponentsManager::renderComponenets[uid] = ComponentsManager::components[uid];
}

void NandGate::onLeftClick(const glm::vec2 &pos) {
    ApplicationState::setSelectedId(m_uid);
}

void NandGate::onRightClick(const glm::vec2 &pos) {
    std::cout << "Right Click on nand gate" << std::endl;
}
} // namespace Bess::Simulator::Components
