#include "components_manager/components_manager.h"

#include "components/clock.h"
#include "components/connection.h"
#include "components/input_probe.h"
#include "components/jcomponent.h"
#include "components/text_component.h"

#include "common/helpers.h"
#include "components/flip_flops/flip_flops.h"
#include "components/output_probe.h"
#include "components_manager/component_bank.h"
#include "pages/main_page/main_page_state.h"

#include <iostream>

namespace Bess::Simulator {

    std::unordered_map<int, uuids::uuid> ComponentsManager::m_renderIdToCId;

    std::unordered_map<uuids::uuid, int> ComponentsManager::m_compIdToRId;

    std::unordered_map<std::string, uuids::uuid> ComponentsManager::m_slotsToConn;

    int ComponentsManager::renderIdCounter;

    std::unordered_map<uuids::uuid, ComponentPtr> ComponentsManager::components;

    std::vector<uuids::uuid> ComponentsManager::renderComponents;

    uuids::uuid ComponentsManager::emptyId;

    const float ComponentsManager::zIncrement = 0.0001f;

    float ComponentsManager::zPos;

    void ComponentsManager::init() {
        ComponentsManager::emptyId = Common::Helpers::uuidGenerator.getUUID();
        reset();
    }

    void ComponentsManager::generateComponent(ComponentType type, const std::any &data, const glm::vec3 &pos) {
        switch (type) {
        case ComponentType::jcomponent: {
            const auto val = std::any_cast<const std::shared_ptr<Components::JComponentData>>(data);
            Components::JComponent().generate(val, pos);
        } break;
        case ComponentType::inputProbe: {
            Components::InputProbe().generate(pos);
        } break;
        case ComponentType::outputProbe: {
            Components::OutputProbe().generate(pos);
        } break;
        case ComponentType::text: {
            Components::TextComponent().generate(pos);
        } break;
        case ComponentType::clock: {
            Components::Clock().generate(pos);
        } break;
        case ComponentType::flipFlop: {
            if (const auto name = std::any_cast<std::string>(data); name == Components::JKFlipFlop::name)
                Components::JKFlipFlop().generate(pos);
            else if (name == Components::DFlipFlop::name)
                Components::DFlipFlop().generate(pos);
            // else if (name == Components::SRFlipFlop::name)
            //     Components::SRFlipFlop().generate(pos);
        } break;
        default:
            throw std::runtime_error("Component type not registered in components manager " + std::to_string(static_cast<int>(type)));
        }
    }

    void ComponentsManager::generateComponent(const ComponentBankElement &comp, const glm::vec3 &pos) {
        std::any data = NULL;
        if (comp.getType() == Simulator::ComponentType::jcomponent) {
            data = comp.getJCompData();
        } else if (comp.getType() == Simulator::ComponentType::flipFlop) {
            data = comp.getName();
        }
        generateComponent(comp.getType(), data, pos);
    }

    void ComponentsManager::deleteComponent(const uuids::uuid uid) {
        if (uid.is_nil() || !components.contains(uid))
            return;

        if (const auto renderIt = std::ranges::find(renderComponents, uid); renderIt != renderComponents.end()) {
            renderComponents.erase(renderIt);
        }
        if (const auto state = Pages::MainPageState::getInstance(); state->isBulkIdPresent(uid))
            state->removeBulkId(uid, false);

        m_renderIdToCId.erase(m_compIdToRId[uid]);
        m_compIdToRId.erase(uid);
        components[uid]->deleteComponent();

        components.erase(uid);
    }

    uuids::uuid ComponentsManager::addConnection(const uuids::uuid &start, const uuids::uuid &end) {
        const auto slotA = getComponent<Components::Slot>(start);
        const auto slotB = getComponent<Components::Slot>(end);

        std::shared_ptr<Components::Slot> outputSlot, inputSlot;

        if (slotA->getType() == ComponentType::outputSlot) {
            inputSlot = slotB;
            outputSlot = slotA;
        } else {
            inputSlot = slotA;
            outputSlot = slotB;
        }

        const auto iId = inputSlot->getId();
        const auto oId = outputSlot->getId();

        if (outputSlot->isConnectedTo(iId))
            return emptyId;

        outputSlot->addConnection(iId);
        inputSlot->addConnection(oId);

        // adding interactive wire
        return Components::Connection::generate(iId, oId);
    }

    const uuids::uuid &ComponentsManager::renderIdToCid(const int rId) {
        if (!m_renderIdToCId.contains(rId)) {
            std::cout << "Render Id not found " << rId << std::endl;
            assert(false);
        }
        return m_renderIdToCId[rId];
    }

    bool ComponentsManager::isRenderIdPresent(const int rId) {
        return m_renderIdToCId.contains(rId);
    }

    int ComponentsManager::compIdToRid(const uuids::uuid &cid) {
        return m_compIdToRId[cid];
    }

    void ComponentsManager::addRenderIdToCId(const int rid, const uuids::uuid &cid) {
        m_renderIdToCId[rid] = cid;
    }

    void ComponentsManager::addCompIdToRId(const int rid, const uuids::uuid &cid) {
        m_compIdToRId[cid] = rid;
    }

    void ComponentsManager::addSlotsToConn(const uuids::uuid &inpSlot, const uuids::uuid &outSlot, const uuids::uuid &conn) {
        const std::string key = Common::Helpers::uuidToStr(inpSlot) + "," + Common::Helpers::uuidToStr(outSlot);
        m_slotsToConn[key] = conn;
    }

    const uuids::uuid &ComponentsManager::getConnectionBetween(const uuids::uuid &inpSlot, const uuids::uuid &outSlot) {
        const std::string key = Common::Helpers::uuidToStr(inpSlot) + "," + Common::Helpers::uuidToStr(outSlot);
        return m_slotsToConn[key];
    }

    const uuids::uuid &ComponentsManager::getConnectionBetween(const std::string &inputOutputSlot) {
        return m_slotsToConn[inputOutputSlot];
    }

    void ComponentsManager::removeSlotsToConn(const uuids::uuid &inpSlot, const uuids::uuid &outSlot) {
        const std::string key = Common::Helpers::uuidToStr(inpSlot) + "," + Common::Helpers::uuidToStr(outSlot);
        if (!m_slotsToConn.contains(key))
            return;
        m_slotsToConn.erase(key);
    }

    const std::string &ComponentsManager::getSlotsForConnection(const uuids::uuid &conn) {
        // extracts key from m_slotsToConn based on conn id
        for (auto &[key, value] : m_slotsToConn) {
            if (value == conn)
                return key;
        }

        throw std::runtime_error("Abandoned Connection");
    }

    int ComponentsManager::getNextRenderId() { return renderIdCounter++; }

    void ComponentsManager::reset() {
        zPos = 0.0f;
        renderIdCounter = 0;
        components.clear();
        renderComponents.clear();
        m_compIdToRId.clear();
        m_renderIdToCId.clear();
        m_compIdToRId[emptyId] = -1;
        m_renderIdToCId[-1] = emptyId;
    }

    float ComponentsManager::getNextZPos() {
        zPos += zIncrement;
        return zPos;
    }

    bool ComponentsManager::isRenderComponent(const int rId) {
        return std::ranges::find(renderComponents, m_renderIdToCId[rId]) != renderComponents.end();
    }
} // namespace Bess::Simulator
