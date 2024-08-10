#include "components/jcomponent.h"
#include "components/connection.h"

#include "components_manager/component_bank.h"

#include "application_state.h"
#include "glm.hpp"
#include "renderer/renderer.h"

#include "common/theme.h"
#include "common/helpers.h"

#include "simulator/simulator_engine.h"

namespace Bess::Simulator::Components {

#define BIND_EVENT_FN_1(fn)                                                    \
    std::bind(&JComponent::fn, this, std::placeholders::_1)

    glm::vec2 gateSize = { 142.f, 100.f };

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

    void JComponent::drawBackground(const glm::vec4& borderThicknessPx, float rPx, float headerHeight) {
        bool selected = ApplicationState::getSelectedId() == m_uid;

        auto borderColor = selected ? glm::vec4(Theme::selectedCompColor, 1.f) : Theme::componentBorderColor;

        Renderer2D::Renderer::quad(
            m_position,
            gateSize,
            Theme::componentBGColor,
            m_renderId,
            glm::vec4(rPx),
            borderColor,
            borderThicknessPx
        );

        auto headerPos = m_position;
        headerPos.y = m_position.y + ((gateSize.y / 2) - (headerHeight / 2.f));

        Renderer2D::Renderer::quad(
            headerPos,
            { gateSize.x, headerHeight },
            Theme::compHeaderColor,
            m_renderId,
            glm::vec4(rPx, rPx, 0.f, 0.f)
       );

    }

    std::string decodeExpr(const std::string& str) {
        std::string exp = "";
        for (auto ch : str) {
            if (std::isdigit(ch)) ch = 'A' + (ch - '0');
            exp += ch;
        }
        return exp;
    }
    void JComponent::render() {
        bool selected = ApplicationState::getSelectedId() == m_uid;
        float rPx = 16.f;

        glm::vec4 borderThicknessPx({1.f, 1.f, 1.f, 1.f});
        float headerHeight = 20.f;
        glm::vec2 slotRowPadding = { 4.0f, 4.f };
        glm::vec2 gatePadding = { 4.0f, 4.f };
        float rowGap = 4.f;
        float sCharHeight = Renderer2D::Renderer::getCharRenderSize('Y', 12.f).y;
        float rowHeight = (slotRowPadding.y * 2) + sCharHeight;

        float maxSlotsCount = std::max(m_inputSlots.size(), m_outputSlots.size());

        gateSize.y = headerHeight + (rowHeight + rowGap) * maxSlotsCount + 4.f;

        drawBackground(borderThicknessPx, rPx, headerHeight);

        auto leftCornerPos = Common::Helpers::GetLeftCornerPos(m_position, gateSize);
        {
            glm::vec3 inpSlotRowPos = { leftCornerPos.x + 8.f + gatePadding.x, leftCornerPos.y - headerHeight - 4.f, leftCornerPos.z };

            for (int i = 0; i < m_inputSlots.size(); i++) {
                char ch = 'A' + i;
                auto charSize = Renderer2D::Renderer::getCharRenderSize(ch, 12.f);

                auto height = (slotRowPadding.y * 2) + charSize.y;

                auto pos = inpSlotRowPos;
                pos.y -= height / 2.f;

                Slot* slot = (Slot*)Simulator::ComponentsManager::components[m_inputSlots[i]].get();
                slot->update(pos, {12.f, 0.f}, std::string(1, ch));
                slot->render();

                inpSlotRowPos.y -= height + rowGap;
            }
        }
        
        {
            glm::vec3 outSlotRowPos = { leftCornerPos.x + gateSize.x - 8.f - gatePadding.x, leftCornerPos.y - headerHeight - 4.f, leftCornerPos.z };

            for (int i = 0; i < m_outputSlots.size(); i++) {
                auto height = rowHeight;

                auto pos = outSlotRowPos;
                pos.y -= height / 2.f;

                Slot* slot = (Slot*)Simulator::ComponentsManager::components[m_outputSlots[i]].get();
                auto& expr = m_data->getOutputs()[i];
                slot->update(pos, {-12.f, 0.f}, decodeExpr(expr));
                slot->render();

                outSlotRowPos.y -= height + rowGap;
            }
        }

        Renderer2D::Renderer::text(m_name, leftCornerPos + glm::vec3({ 8.f, -8.f - (sCharHeight / 2.f), ComponentsManager::zIncrement}), 11.f, {1.f, 1.f, 1.f}, m_renderId);
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

        ComponentsManager::renderComponenets.emplace_back(uid);
    }

    nlohmann::json JComponent::toJson()
    {
        nlohmann::json data;

        data["uid"] = m_uid.str();
        data["pos"] = Common::Helpers::EncodeVec3(m_position);
        data["type"] = (int)m_type;

        data["jCompData"]["collection"] = m_data->getCollectionName();
        data["jCompData"]["name"] = m_data->getName();

        for (auto& uid : m_inputSlots) data["inputSlots"].emplace_back(uid.str());
        for (auto& uid : m_outputSlots) data["outputSlots"].emplace_back(uid.str());

        for (auto& uid : m_outputSlots) {
            auto slot = (Slot*)ComponentsManager::components[uid].get();
            auto str = uid.str();
            for (auto& cid : slot->getConnections())
                data["connections"][str].emplace_back(cid.str());
        }

        return data;
    }

    void JComponent::fromJson(const nlohmann::json& data)
    {
        UUIDv4::UUID uid;
        uid.fromStr((static_cast<std::string>(data["uid"])).c_str());

        auto pos = Common::Helpers::DecodeVec3(data["pos"]);

        auto jCompData = Simulator::ComponentBank::getJCompData(data["jCompData"]["collection"], data["jCompData"]["name"]);
        
        std::vector<UUIDv4::UUID> inputSlots = {};
        std::vector<UUIDv4::UUID> outputSlots = {};
        for(std::string sidStr: data["inputSlots"]) {
            UUIDv4::UUID sid;
            sid.fromStr(sidStr.c_str());
            inputSlots.emplace_back(sid);
            auto renderId = ComponentsManager::getNextRenderId();
            ComponentsManager::components[sid] = std::make_shared<Components::Slot>(
                sid, uid, renderId, ComponentType::inputSlot);
            ComponentsManager::addRenderIdToCId(renderId, sid);
            ComponentsManager::addCompIdToRId(renderId, sid);
        }

        for (std::string sidStr: data["outputSlots"]) {
            UUIDv4::UUID sid;
            sid.fromStr(sidStr.c_str());
            outputSlots.emplace_back(sid);
            auto renderId = ComponentsManager::getNextRenderId();
            auto slot = std::make_shared<Components::Slot>(sid, uid, renderId, ComponentType::outputSlot);
            ComponentsManager::components[sid] = slot;
            ComponentsManager::addRenderIdToCId(renderId, sid);
            ComponentsManager::addCompIdToRId(renderId, sid);

            if (!data.contains("connections") || !data["connections"].contains(sidStr)) continue;

            for (std::string cidStr : data["connections"][sidStr]) {
                UUIDv4::UUID cid;
                cid.fromStr(cidStr.c_str());
                slot->addConnection(cid, false);
                Components::Connection().generate(cid, sid);
            }
        }

        auto renderId = ComponentsManager::getNextRenderId();

        ComponentsManager::components[uid] = std::make_shared<Components::JComponent>(
            uid, renderId, pos, inputSlots, outputSlots, jCompData);

        ComponentsManager::addRenderIdToCId(renderId, uid);
        ComponentsManager::addCompIdToRId(renderId, uid);

        ComponentsManager::renderComponenets.emplace_back(uid);
    }

    void JComponent::onLeftClick(const glm::vec2& pos) {
        ApplicationState::setSelectedId(m_uid);
    }

    void JComponent::onRightClick(const glm::vec2& pos) {
        std::cout << "Right Click on jcomp gate" << std::endl;
    }


} // namespace Bess::Simulator::Components
