#include "components_manager/components_manager.h"

#include "components/connection.h"
#include "components/input_probe.h"
#include "components/jcomponent.h"

#include "application_state.h"
#include "components/output_probe.h"
#include "common/helpers.h"

namespace Bess::Simulator {

    std::unordered_map<int, UUIDv4::UUID> ComponentsManager::renderIdToCId;

    std::unordered_map<UUIDv4::UUID, int> ComponentsManager::compIdToRId;

    int ComponentsManager::renderIdCounter;

    std::unordered_map<UUIDv4::UUID, ComponentsManager::ComponentPtr>
        ComponentsManager::components;

    std::unordered_map<UUIDv4::UUID, ComponentsManager::ComponentPtr>
        ComponentsManager::renderComponenets;
    UUIDv4::UUID ComponentsManager::emptyId;

    const float ComponentsManager::zIncrement = 0.0001f;

    float ComponentsManager::zPos = 0.0f;


    void ComponentsManager::init() {
       ComponentsManager::emptyId = Common::Helpers::uuidGenerator.getUUID();
        compIdToRId[emptyId] = -1;
        renderIdToCId[-1] = emptyId;
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
        }
    }

    void ComponentsManager::addConnection(const UUIDv4::UUID& slot1, const UUIDv4::UUID& slot2) {
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

        if (outputSlot->isConnectedTo(inputSlot->getId())) return;

        // adding only to output slot to maintain one way flow
        outputSlot->addConnection(inputSlot->getId());

        // adding interative wire
        Components::Connection().generate(slot1, slot2);
    }

    const UUIDv4::UUID& ComponentsManager::renderIdToCid(int rId) {
        return renderIdToCId[rId];
    }

    int ComponentsManager::compIdToRid(const UUIDv4::UUID& uId) {
        return compIdToRId[uId];
    }

    void ComponentsManager::addRenderIdToCId(int rid, const UUIDv4::UUID& cid)
    {
        renderIdToCId[rid] = cid;
    }

    void ComponentsManager::addCompIdToRId(int rid, const UUIDv4::UUID& cid)
    {
        compIdToRId[cid] = rid;
    }

    int ComponentsManager::getNextRenderId() { return renderIdCounter++; }
    
    float ComponentsManager::getNextZPos() {
        zPos += zIncrement;
        return zPos;
    }
} // namespace Bess::Simulator
