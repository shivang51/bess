#include "components_manager/components_manager.h"

#include "components/connection.h"
#include "components/input_probe.h"
#include "components/jcomponent.h"
#include "components/text_component.h"

#include "application_state.h"
#include "components/output_probe.h"
#include "common/helpers.h"

namespace Bess::Simulator {

    std::unordered_map<int, uuids::uuid> ComponentsManager::m_renderIdToCId;

    std::unordered_map<uuids::uuid, int> ComponentsManager::m_compIdToRId;

    std::unordered_map<std::string, uuids::uuid> ComponentsManager::m_slotsToConn;

    int ComponentsManager::renderIdCounter;

    std::unordered_map<uuids::uuid, ComponentPtr> ComponentsManager::components;

    std::vector<uuids::uuid> ComponentsManager::renderComponenets;
    
    uuids::uuid ComponentsManager::emptyId;

    const float ComponentsManager::zIncrement = 0.0001f;

    float ComponentsManager::zPos;


    void ComponentsManager::init() {
        ComponentsManager::emptyId = Common::Helpers::uuidGenerator.getUUID();
        reset();
    }

    void ComponentsManager::generateComponent(ComponentType type, const std::any& data, const glm::vec3& pos) {
        switch (type) {
        case ComponentType::jcomponent: {
            auto val = std::any_cast<const std::shared_ptr<Components::JComponentData>>(data);
            Components::JComponent().generate(val, pos);
        }break;
        case ComponentType::inputProbe: {
            Components::InputProbe().generate(pos);
        }break;
        case ComponentType::outputProbe: {
            Components::OutputProbe().generate(pos);
        }break;
        case ComponentType::text: {
            Components::TextComponent().generate(pos);
        }break;
        }
    }

    void ComponentsManager::deleteComponent(const uuids::uuid cid)
    {
        if (cid.is_nil() || components.find(cid) == components.end()) return;

        auto renderIt = std::find(renderComponenets.begin(), renderComponenets.end(), cid);
        if (renderIt != renderComponenets.end()) {
            ApplicationState::setSelectedId(emptyId, false);
            renderComponenets.erase(renderIt);
        }

        m_renderIdToCId.erase(m_compIdToRId[cid]);
        m_compIdToRId.erase(cid);
        components[cid]->deleteComponent();
        
        components.erase(cid);
    }

    void ComponentsManager::addConnection(const uuids::uuid& slot1, const uuids::uuid& slot2) {
        auto slotA = (Bess::Simulator::Components::Slot*)components[slot1].get();
        auto slotB = (Bess::Simulator::Components::Slot*)components[slot2].get();

        Bess::Simulator::Components::Slot* outputSlot, *inputSlot;

        if (slotA->getType() == ComponentType::outputSlot) {
            inputSlot = slotB;
            outputSlot = slotA;
        }
        else {
            inputSlot = slotA;
            outputSlot = slotB;
        }

        auto iId = inputSlot->getId();
        auto oId = outputSlot->getId();

        if (outputSlot->isConnectedTo(iId)) return;

        outputSlot->addConnection(iId);
        inputSlot->addConnection(oId);

        // adding interative wire
        Components::Connection::generate(iId, oId);
    }

    const uuids::uuid& ComponentsManager::renderIdToCid(int rId) {
        return m_renderIdToCId[rId];
    }

    bool ComponentsManager::isRenderIdPresent(int rId)
    {
        return m_renderIdToCId.find(rId) != m_renderIdToCId.end();
    }

    int ComponentsManager::compIdToRid(const uuids::uuid& uId) {
        return m_compIdToRId[uId];
    }

    void ComponentsManager::addRenderIdToCId(int rid, const uuids::uuid& cid)
    {
        m_renderIdToCId[rid] = cid;
    }

    void ComponentsManager::addCompIdToRId(int rid, const uuids::uuid& cid)
    {
        m_compIdToRId[cid] = rid;
    }

    void ComponentsManager::addSlotsToConn(const uuids::uuid& inpSlot, const uuids::uuid& outSlot, const uuids::uuid& conn)
    {
        std::string key = Common::Helpers::uuidToStr(inpSlot) + "," + Common::Helpers::uuidToStr(outSlot);
        m_slotsToConn[key] = conn;
    }

    const uuids::uuid& ComponentsManager::getConnectionBetween(const uuids::uuid& inpSlot, const uuids::uuid& outSlot) {
        std::string key = Common::Helpers::uuidToStr(inpSlot) + "," + Common::Helpers::uuidToStr(outSlot);
        return m_slotsToConn[key];
    }

    void ComponentsManager::removeSlotsToConn(const uuids::uuid& inpSlot, const uuids::uuid& outSlot)
    {
        std::string key = Common::Helpers::uuidToStr(inpSlot) + "," + Common::Helpers::uuidToStr(outSlot);
        if (m_slotsToConn.find(key) == m_slotsToConn.end()) return;
        m_slotsToConn.erase(key);
    }

    int ComponentsManager::getNextRenderId() { return renderIdCounter++; }

    void ComponentsManager::reset()
    {
        zPos = 0.0f;
        renderIdCounter = 0;
        components.clear();
        renderComponenets.clear();
        m_compIdToRId.clear();
        m_renderIdToCId.clear();
        m_compIdToRId[emptyId] = -1;
        m_renderIdToCId[-1] = emptyId;
    }
    
    float ComponentsManager::getNextZPos() {
        zPos += zIncrement;
        return zPos;
    }
} // namespace Bess::Simulator
