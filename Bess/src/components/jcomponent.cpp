#include "components/jcomponent.h"
#include "components/connection.h"

#include "components_manager/component_bank.h"

#include "application_state.h"
#include "glm.hpp"
#include "renderer/renderer.h"

#include "common/helpers.h"
#include "settings/viewport_theme.h"

#include "simulator/simulator_engine.h"

namespace Bess::Simulator::Components {

#define BIND_EVENT_FN_1(fn) \
    std::bind(&JComponent::fn, this, std::placeholders::_1)

    glm::vec2 GATE_SIZE = {142.f, 100.f};

    JComponent::JComponent() : Component(), m_data{} {
    }

    JComponent::JComponent(const uuids::uuid &uid, int renderId, glm::vec3 position,
                           std::vector<uuids::uuid> inputSlots,
                           std::vector<uuids::uuid> outputSlots, const std::shared_ptr<JComponentData> data)
        : Component(uid, renderId, position, ComponentType::jcomponent), m_data{data} {
        m_name = data->getName();

        m_inputSlots = inputSlots;
        m_outputSlots = outputSlots;

        m_events[ComponentEventType::leftClick] =
            (OnLeftClickCB)BIND_EVENT_FN_1(onLeftClick);
        m_events[ComponentEventType::rightClick] =
            (OnRightClickCB)BIND_EVENT_FN_1(onRightClick);

        simulate();
    }

    void JComponent::drawBackground(const glm::vec4 &borderThicknessPx, float rPx, float headerHeight, const glm::vec2 &gateSize) {
        bool selected = ApplicationState::getSelectedId() == m_uid;

        auto borderColor = selected ? ViewportTheme::selectedCompColor : ViewportTheme::componentBorderColor;

        Renderer2D::Renderer::quad(
            m_position,
            gateSize,
            ViewportTheme::componentBGColor,
            m_renderId,
            glm::vec4(rPx),
            true,
            borderColor,
            borderThicknessPx);

        auto headerPos = m_position;
        headerPos.y = m_position.y + ((gateSize.y / 2) - (headerHeight / 2.f));

        Renderer2D::Renderer::quad(
            headerPos,
            {gateSize.x, headerHeight},
            ViewportTheme::compHeaderColor,
            m_renderId,
            glm::vec4(rPx, rPx, 0.f, 0.f));
    }

    std::string decodeExpr(const std::string &str) {
        std::string exp = "";
        for (auto ch : str) {
            if (std::isdigit(ch))
                ch = 'A' + (ch - '0');
            exp += ch;
        }
        return exp;
    }

    void JComponent::render() {
        bool selected = ApplicationState::getSelectedId() == m_uid;
        float rPx = 16.f;

        glm::vec4 borderThicknessPx({1.f, 1.f, 1.f, 1.f});
        float headerHeight = 20.f;
        glm::vec2 slotRowPadding = {4.0f, 4.f};
        glm::vec2 gatePadding = {4.0f, 4.f};
        float labelGap = 8.f;
        float rowGap = 4.f;
        auto sampleCharSize = Renderer2D::Renderer::getCharRenderSize('Z', 12.f);
        float sCharHeight = sampleCharSize.y;
        float rowHeight = (slotRowPadding.y * 2) + sCharHeight;

        float maxSlotsCount = std::max(m_inputSlots.size(), m_outputSlots.size());
        float maxWidth = 0.f;
        for (auto &expr : m_data->getOutputs()) {
            int l = expr.size();
            float width = l * sampleCharSize.x;
            maxWidth = std::fmaxf(width, maxWidth);
        }

        maxWidth += labelGap + 8.f + sampleCharSize.x + 16.f + (gatePadding.x * 2.f);

        auto gateSize_ = GATE_SIZE;

        if (maxWidth > gateSize_.x) {
            gateSize_.x += maxWidth - gateSize_.x + 16.f;
        }

        gateSize_.y = headerHeight + (rowHeight + rowGap) * maxSlotsCount + 4.f;

        drawBackground(borderThicknessPx, rPx, headerHeight, gateSize_);

        auto leftCornerPos = Common::Helpers::GetLeftCornerPos(m_position, gateSize_);

        {
            glm::vec3 inpSlotRowPos = {leftCornerPos.x + 8.f + gatePadding.x, leftCornerPos.y - headerHeight - 4.f, leftCornerPos.z};

            for (int i = 0; i < m_inputSlots.size(); i++) {
                char ch = 'A' + i;

                auto height = (slotRowPadding.y * 2) + sCharHeight;

                auto pos = inpSlotRowPos;
                pos.y -= height / 2.f;

                Slot *slot = (Slot *)Simulator::ComponentsManager::components[m_inputSlots[i]].get();
                slot->update(pos, {labelGap, 0.f}, std::string(1, ch));
                slot->render();

                inpSlotRowPos.y -= height + rowGap;
            }
        }

        {
            glm::vec3 outSlotRowPos = {leftCornerPos.x + gateSize_.x - 8.f - gatePadding.x, leftCornerPos.y - headerHeight - 4.f, leftCornerPos.z};

            for (int i = 0; i < m_outputSlots.size(); i++) {
                auto height = rowHeight;

                auto pos = outSlotRowPos;
                pos.y -= height / 2.f;

                Slot *slot = (Slot *)Simulator::ComponentsManager::components[m_outputSlots[i]].get();
                auto &expr = m_data->getOutputs()[i];
                slot->update(pos, {-labelGap, 0.f}, decodeExpr(expr));
                slot->render();

                outSlotRowPos.y -= height + rowGap;
            }
        }

        Renderer2D::Renderer::text(m_name, leftCornerPos + glm::vec3({8.f, -8.f - (sCharHeight / 2.f), ComponentsManager::zIncrement}), 11.f, ViewportTheme::textColor, m_renderId);
    }

    void JComponent::simulate() {
        std::vector<int> inputs = {};
        std::vector<int> outputs = {};

        for (auto &slotId : m_inputSlots) {
            auto slot = (Slot *)ComponentsManager::components[slotId].get();
            auto v = static_cast<std::underlying_type_t<DigitalState>>(slot->getState());
            inputs.emplace_back(v);
        }

        for (auto &exp : m_data->getOutputs()) {
            int v = Simulator::Engine::evaluateExpression(exp, inputs);
            outputs.emplace_back(v);
        }

        for (int i = 0; i < outputs.size(); i++) {
            auto &slotId = m_outputSlots[i];
            DigitalState outputState = static_cast<DigitalState>(outputs[i]);
            Simulator::Engine::addToSimQueue(slotId, m_uid, outputState);
        }
    }

    void JComponent::generate(const glm::vec3 &pos) {
    }

    void JComponent::generate(const std::shared_ptr<JComponentData> data, const glm::vec3 &pos) {
        auto pId = Common::Helpers::uuidGenerator.getUUID();
        // input slots
        int n = data->getInputCount();
        std::vector<uuids::uuid> inputSlots;
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
        std::vector<uuids::uuid> outputSlots;
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

        auto &uid = pId;
        auto renderId = ComponentsManager::getNextRenderId();

        auto pos_ = pos;
        pos_.z = ComponentsManager::getNextZPos();

        ComponentsManager::components[uid] = std::make_shared<Components::JComponent>(uid, renderId, pos_, inputSlots, outputSlots, data);

        ComponentsManager::addRenderIdToCId(renderId, uid);
        ComponentsManager::addCompIdToRId(renderId, uid);

        ComponentsManager::renderComponenets.emplace_back(uid);
    }

    void JComponent::deleteComponent() {
        for (auto &id : m_inputSlots) {
            ComponentsManager::deleteComponent(id);
        }

        for (auto &id : m_outputSlots) {
            ComponentsManager::deleteComponent(id);
        }
    }

    nlohmann::json JComponent::toJson() {
        nlohmann::json data;

        data["uid"] = Common::Helpers::uuidToStr(m_uid);
        data["pos"] = Common::Helpers::EncodeVec3(m_position);
        data["type"] = (int)m_type;

        data["jCompData"]["collection"] = m_data->getCollectionName();
        data["jCompData"]["name"] = m_data->getName();

        for (auto &uid : m_inputSlots) {
            auto slot = (Slot *)ComponentsManager::components[uid].get();
            data["inputSlots"].emplace_back(slot->toJson());
        }

        for (auto &uid : m_outputSlots) {
            auto slot = (Slot *)ComponentsManager::components[uid].get();
            data["outputSlots"].emplace_back(slot->toJson());
        }

        return data;
    }

    void JComponent::fromJson(const nlohmann::json &data) {
        uuids::uuid uid;
        uid = Common::Helpers::strToUUID(static_cast<std::string>(data["uid"]));

        auto pos = Common::Helpers::DecodeVec3(data["pos"]);

        auto jCompData = Simulator::ComponentBank::getJCompData(data["jCompData"]["collection"], data["jCompData"]["name"]);

        std::vector<uuids::uuid> inputSlots = {};
        for (auto &slotJson : data["inputSlots"]) {
            inputSlots.emplace_back(Slot::fromJson(slotJson, uid));
        }

        std::vector<uuids::uuid> outputSlots = {};
        for (auto &slotJson : data["outputSlots"]) {
            outputSlots.emplace_back(Slot::fromJson(slotJson, uid));
        }

        auto renderId = ComponentsManager::getNextRenderId();

        ComponentsManager::components[uid] = std::make_shared<Components::JComponent>(uid, renderId, pos, inputSlots, outputSlots, jCompData);

        ComponentsManager::addRenderIdToCId(renderId, uid);
        ComponentsManager::addCompIdToRId(renderId, uid);
        ComponentsManager::renderComponenets.emplace_back(uid);
    }

    void JComponent::onLeftClick(const glm::vec2 &pos) {
        ApplicationState::setSelectedId(m_uid);
    }

    void JComponent::onRightClick(const glm::vec2 &pos) {
        std::cout << "Right Click on jcomp gate" << std::endl;
    }

} // namespace Bess::Simulator::Components
