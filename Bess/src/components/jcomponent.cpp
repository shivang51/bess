#include "components/jcomponent.h"

#include "application_state.h"
#include "fwd.hpp"
#include "renderer/renderer.h"

#include "common/theme.h"
#include "common/helpers.h"

#include "simulator/simulator_engine.h"

namespace Bess::Simulator::Components {

#define BIND_EVENT_FN_1(fn)                                                    \
    std::bind(&JComponent::fn, this, std::placeholders::_1)

    glm::vec2 gateSize = { 150.f, 100.f };

    JComponent::JComponent() : Component(), m_data{}
    {
    }

    JComponent::JComponent(const UUIDv4::UUID& uid, int renderId, glm::vec3 position,
        std::vector<UUIDv4::UUID> inputSlots,
        std::vector<UUIDv4::UUID> outputSlots, const std::shared_ptr<JComponentData> data)
        : Component(uid, renderId, position, ComponentType::jcomponent), m_data{data} {
        m_name = data->getName();

        m_inputSlots = inputSlots;
        m_outputSlots = outputSlots;

        m_events[ComponentEventType::leftClick] =
            (OnLeftClickCB)BIND_EVENT_FN_1(onLeftClick);
        m_events[ComponentEventType::rightClick] =
            (OnRightClickCB)BIND_EVENT_FN_1(onRightClick);
    }

    void JComponent::render() {
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
            { m_position.x,
             m_position.y + ((gateSize.y / 2) - 10.f) - borderThickness / 2.f,
             m_position.z },
            { gateSize.x - borderThickness, 20.f }, Theme::compHeaderColor,
            m_renderId, glm::vec4(r1, r1, 0.f, 0.f),
            glm::vec4(Theme::compHeaderColor, 1.f), borderThickness / 20.f);

        std::vector<glm::vec3> slots = {
            glm::vec3({-(gateSize.x / 2) + 10.f, 6.f, 0.f}),
            glm::vec3({-(gateSize.x / 2) + 10.f, -(gateSize.y / 2) + 16.0, 0.f}) };

        for (int i = 0; i < m_inputSlots.size(); i++) {
            Slot* slot =
                (Slot*)Simulator::ComponentsManager::components[m_inputSlots[i]]
                .get();
            slot->update(m_position + slots[i]);
            slot->render();

            Renderer2D::Renderer::text(std::string(1, 'A' + i), m_position + slots[i] + glm::vec3({ 10.f, -2.5f, ComponentsManager::zIncrement }), 12.f, { 1.f, 1.f, 1.f }, m_renderId);

        }

        for (auto& slot : slots) {
            slot.x = gateSize.x / 2.f - 10.f, -10.f;
        }


        for (int i = 0; i < m_outputSlots.size(); i++) {
            Slot* slot =
                (Slot*)Simulator::ComponentsManager::components[m_outputSlots[i]]
                .get();
            slot->update(m_position + slots[i]);
            slot->render();
        }


        Renderer2D::Renderer::text(m_name, Common::Helpers::GetLeftCornerPos(m_position, gateSize) + glm::vec3({ 8.f, -16.f, ComponentsManager::zIncrement }), 12.f, { 1.f, 1.f, 1.f }, m_renderId);
    }

    void JComponent::simulate()
    {
        std::vector<int> inputs = {};
        std::vector<int> outputs = {};


        for (auto& slotId : m_inputSlots) {
            auto slot = (Slot*)ComponentsManager::components[slotId].get();
            auto v = static_cast<std::underlying_type_t<DigitalState>>(slot->getState());
            inputs.emplace_back(v);
        }

        for (auto& exp : m_data->getOutputs()) {
            int v = Simulator::Engine::evaluateExpression(exp, inputs);
            outputs.emplace_back(v);
        }

        for (int i = 0; i < outputs.size(); i++) {
            auto &slotId = m_outputSlots[i];
            DigitalState outputState = static_cast<DigitalState>(outputs[i]);
            auto slot = (Slot*)ComponentsManager::components[slotId].get();
            slot->setState(m_uid, outputState);
        }
    }

    void JComponent::generate(const glm::vec3& pos)
    {
        
    }

    void JComponent::generate(const std::shared_ptr<JComponentData> data, const glm::vec3& pos)
    {
        auto pId = Common::Helpers::uuidGenerator.getUUID();
        // input slots
        int n = data->getInputCount();
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
        n = data->getOutputs().size();
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

        ComponentsManager::components[uid] = std::make_shared<Components::JComponent>(
            uid, renderId, pos_, inputSlots, outputSlots, data);

        ComponentsManager::addRenderIdToCId(renderId, uid);
        ComponentsManager::addCompIdToRId(renderId, uid);

        ComponentsManager::renderComponenets[uid] = ComponentsManager::components[uid];
    }

    void JComponent::onLeftClick(const glm::vec2& pos) {
        ApplicationState::setSelectedId(m_uid);
    }

    void JComponent::onRightClick(const glm::vec2& pos) {
        std::cout << "Right Click on nand gate" << std::endl;
    }
} // namespace Bess::Simulator::Components
