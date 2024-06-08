#include "components_manager/components_manager.h"

#include "components/connection.h"
#include "components/input_probe.h"
#include "components/nand_gate.h"

#include "application_state.h"
#include "components/output_probe.h"

namespace Bess::Simulator {

std::unordered_map<int, UUIDv4::UUID> ComponentsManager::renderIdToCId;

std::unordered_map<UUIDv4::UUID, int> ComponentsManager::compIdToRId;

int ComponentsManager::renderIdCounter;

UUIDv4::UUIDGenerator<std::mt19937_64> ComponentsManager::uuidGenerator;

std::unordered_map<UUIDv4::UUID, ComponentsManager::ComponentPtr>
    ComponentsManager::components;

std::unordered_map<UUIDv4::UUID, ComponentsManager::ComponentPtr>
    ComponentsManager::renderComponenets;

const UUIDv4::UUID ComponentsManager::emptyId = uuidGenerator.getUUID();

void ComponentsManager::init() {
    compIdToRId[emptyId] = -1;
    renderIdToCId[-1] = emptyId;
}

void ComponentsManager::generateNandGate(const glm::vec3 &pos) {
    // input slots
    int n = 2;
    std::vector<UUIDv4::UUID> inputSlots;
    while (n--) {
        auto uid = uuidGenerator.getUUID();
        auto renderId = getRenderId();
        components[uid] = std::make_shared<Components::Slot>(
            uid, renderId, ComponentType::inputSlot);
        renderIdToCId[renderId] = uid;
        compIdToRId[uid] = renderId;
        inputSlots.emplace_back(uid);
    }

    // output slots
    std::vector<UUIDv4::UUID> outputSlots;
    n = 1;
    while (n--) {
        auto uid = uuidGenerator.getUUID();
        auto renderId = getRenderId();
        components[uid] = std::make_shared<Components::Slot>(
            uid, renderId, ComponentType::outputSlot);
        renderIdToCId[renderId] = uid;
        compIdToRId[uid] = renderId;
        outputSlots.emplace_back(uid);
    }

    auto uid = uuidGenerator.getUUID();
    auto renderId = getRenderId();

    auto pos_ = pos;
    pos_.z = (float)renderComponenets.size();

    components[uid] = std::make_shared<Components::NandGate>(
        uid, renderId, pos_, inputSlots, outputSlots);
    renderIdToCId[renderId] = uid;
    compIdToRId[uid] = renderId;

    renderComponenets[uid] = components[uid];
}

void ComponentsManager::generateInputProbe(const glm::vec3 &pos) {
    auto slotId = uuidGenerator.getUUID();
    auto renderId = getRenderId();
    components[slotId] = std::make_shared<Components::Slot>(
        slotId, renderId, ComponentType::outputSlot);
    renderIdToCId[renderId] = slotId;
    compIdToRId[slotId] = renderId;

    auto uid = uuidGenerator.getUUID();
    auto pos_ = pos;
    pos_.z = (float)renderComponenets.size();

    renderId = getRenderId();
    components[uid] =
        std::make_shared<Components::InputProbe>(uid, renderId, pos_, slotId);
    renderIdToCId[renderId] = uid;
    compIdToRId[uid] = renderId;
    renderComponenets[uid] = components[uid];
}

void ComponentsManager::generateOutputProbe(const glm::vec3 &pos) {
    auto slotId = uuidGenerator.getUUID();
    auto renderId = getRenderId();
    components[slotId] = std::make_shared<Components::Slot>(
        slotId, renderId, ComponentType::inputSlot);
    renderIdToCId[renderId] = slotId;
    compIdToRId[slotId] = renderId;

    auto uid = uuidGenerator.getUUID();
    auto pos_ = pos;
    pos_.z = (float)renderComponenets.size();

    renderId = getRenderId();
    components[uid] =
        std::make_shared<Components::OutputProbe>(uid, renderId, pos_, slotId);
    renderIdToCId[renderId] = uid;
    compIdToRId[uid] = renderId;
    renderComponenets[uid] = components[uid];
}

void ComponentsManager::generateConnection(const UUIDv4::UUID &slot1,
                                           const UUIDv4::UUID &slot2) {
    // tells the slots to which slot they are getting connected
    auto slotA = (Bess::Simulator::Components::Slot *)components[slot1].get();
    slotA->addConnection(slot2);
    auto slotB = (Bess::Simulator::Components::Slot *)components[slot2].get();
    slotB->addConnection(slot1);

    auto uid = uuidGenerator.getUUID();
    auto renderId = getRenderId();
    components[uid] = std::make_shared<Components::Connection>(
        uid, renderId, slotA->getId(), slot2);
    renderIdToCId[renderId] = uid;
    compIdToRId[uid] = renderId;
    renderComponenets[uid] = components[uid];
}

const UUIDv4::UUID &ComponentsManager::renderIdToCid(int rId) {
    return renderIdToCId[rId];
}

int ComponentsManager::compIdToRid(const UUIDv4::UUID &uId) {
    return compIdToRId[uId];
}

// private
int ComponentsManager::getRenderId() { return renderIdCounter++; }
} // namespace Bess::Simulator
