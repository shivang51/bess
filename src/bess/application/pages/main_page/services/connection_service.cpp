#include "connection_service.h"
#include "common/bess_assert.h"
#include "common/bess_uuid.h"
#include "common/logger.h"
#include "component_definition.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/scene_components/connection_scene_component.h"
#include "pages/main_page/scene_components/proxy_slot_component.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "simulation_engine.h"
#include <algorithm>
#include <cstdint>

namespace Bess::Svc {
    SvcConnection &SvcConnection::instance() {
        static SvcConnection instance;
        return instance;
    }

    void SvcConnection::init() {
        m_slotsBin = {};
        BESS_DEBUG("Initialized Connection Service");
    }

    void SvcConnection::destroy() {
        m_slotsBin.clear();
        BESS_DEBUG("Destroyed Connection Service");
    }

    std::vector<UUID> SvcConnection::getDependants(const UUID &connection) {

        auto &sceneState = getScene()->getState();
        if (!sceneState.isComponentValid(connection)) {
            BESS_ERROR("Connection with id {} not found in scene state", (uint64_t)connection);
            return {};
        }

        const auto &connComp = sceneState.getComponentByUuid<Canvas::ConnectionSceneComponent>(connection);

        // Check for slots which can be removed with the connection
        const auto &slotA = getSlot(connComp->getStartSlot());
        const auto &slotB = getSlot(connComp->getEndSlot());

        std::vector<UUID> dependants;

        if (slotA && isSlotRemovable(slotA, 1)) {
            const auto &slotADeps = slotA->getDependants(sceneState);
            dependants.insert(dependants.end(), slotADeps.begin(), slotADeps.end());
            dependants.push_back(slotA->getUuid());
        }

        if (slotB && isSlotRemovable(slotB, 1)) {
            const auto &slotBDeps = slotB->getDependants(sceneState);
            dependants.insert(dependants.end(), slotBDeps.begin(), slotBDeps.end());
            dependants.push_back(slotB->getUuid());
        }

        return dependants;
    }

    /// TODO (shivang): handel connecting to proxys, correctly,
    /// if start was a output slot then take input slot of proxy and connect to it, and vice versa.
    /// correctly add connection to the proxy component
    bool SvcConnection::addConnection(const std::shared_ptr<Canvas::ConnectionSceneComponent> &conn) {
        BESS_DEBUG("Adding connection with uuid {} between slot {} and slot {}",
                   (uint64_t)conn->getUuid(),
                   (uint64_t)conn->getStartSlot(), (uint64_t)conn->getEndSlot());

        if (!conn) {
            BESS_ERROR("[SvcConnection] [addConnection] Invalid connection given");
            return false;
        }

        auto &sceneState = getScene()->getState();

        const auto &slotAId = conn->getStartSlot();
        const auto &slotBId = conn->getEndSlot();

        const auto &[slotA, foundAInScene] = tryFindSlot(slotAId);
        const auto &[slotB, foundBInScene] = tryFindSlot(slotBId);

        if (!slotA) {
            BESS_ERROR("Slot A with id {} of connection {} not found", (uint64_t)slotAId,
                       (uint64_t)conn->getUuid());
            BESS_ASSERT(false, "Slot A of the connection not found");
            return false;
        }

        if (!slotB) {
            BESS_ERROR("Slot B with id {} of connection {} not found", (uint64_t)slotBId,
                       (uint64_t)conn->getUuid());
            BESS_ASSERT(false, "Slot B of the connection not found");
            return false;
        }

        if (foundAInScene && isResizeTriggerSlot(slotA)) {
            BESS_WARN("Connection service cannot handle connection to resize trigger slots yet");
            return false;
        }

        if (!foundAInScene && !addSlot(slotA)) {
            BESS_ERROR("Failed to add slot A with id {} for connection {}", (uint64_t)slotAId,
                       (uint64_t)conn->getUuid());
            BESS_ASSERT(false, "Failed to add slot A for connection");
            return false;
        }

        if (!foundBInScene && isResizeTriggerSlot(slotB)) {
            BESS_WARN("Connection service cannot handle connection to resize trigger slots yet");
            return false;
        }

        if (!foundBInScene && !addSlot(slotB)) {
            BESS_ERROR("Failed to add slot B with id {} for connection {}", (uint64_t)slotBId,
                       (uint64_t)conn->getUuid());
            BESS_ASSERT(false, "Failed to add slot B for connection");
            return false;
        }

        const auto &res = connect(slotAId, slotBId);

        if (res.has_value()) {
            BESS_ERROR("Failed to connect slots {} and {} in sim engine for connection {}, error: {}",
                       (uint64_t)slotAId, (uint64_t)slotBId, (uint64_t)conn->getUuid(), res.value());
            BESS_ASSERT(false, "Failed to connect slots in sim engine for connection");
            return false;
        }

        regConnToComp(slotAId, conn->getUuid());
        regConnToComp(slotBId, conn->getUuid());

        sceneState.addComponent(conn);

        BESS_INFO("[ConnectionSvc] Added connection {} between slot {} and slot {}",
                  (uint64_t)conn->getUuid(),
                  (uint64_t)slotAId, (uint64_t)slotBId);

        return true;
    }

    std::vector<UUID> SvcConnection::removeConnection(const std::shared_ptr<Canvas::ConnectionSceneComponent> &conn) {
        if (!conn) {
            BESS_ERROR("[SvcConnection] [removeConnection] Invalid connection given");
            return {};
        }

        const auto &startSlotId = conn->getStartSlot();
        const auto &endSlotId = conn->getEndSlot();

        const auto &slotA = getSlot(startSlotId);
        BESS_ASSERT(slotA, "Failed to get slot A for connection");

        const auto &slotB = getSlot(endSlotId);
        BESS_ASSERT(slotB, "Failed to get slot B for connection");

        const auto &slotAId = slotA->getUuid();
        const auto &slotBId = slotB->getUuid();

        disconnectInSimEngine(slotAId, slotBId);

        auto &sceneState = getScene()->getState();

        auto removedIds = std::vector<UUID>{};

        // Processing Slot-A
        {
            slotA->removeConnection(conn->getUuid());
            if (isSlotRemovable(slotA)) {
                m_slotsBin[slotA->getUuid()] = slotA;
                if (!removeSlot(slotA)) {
                    BESS_ERROR("Failed to remove slot A with id {} for connection {}", (uint64_t)slotAId,
                               (uint64_t)conn->getUuid());
                    BESS_ASSERT(false, "Failed to remove slot A for connection");
                    return {};
                }
                removedIds.push_back(slotA->getUuid());
            }
        }

        // Processing Slot-B
        {
            slotB->removeConnection(conn->getUuid());
            if (isSlotRemovable(slotB)) {
                m_slotsBin[slotB->getUuid()] = slotB;
                if (!removeSlot(slotB)) {
                    BESS_ERROR("Failed to remove slot B with id {} for connection {}", (uint64_t)slotBId,
                               (uint64_t)conn->getUuid());
                    BESS_ASSERT(false, "Failed to remove slot B for connection");
                    return {};
                }
                removedIds.push_back(slotB->getUuid());
            }
        }

        sceneState.removeComponent(conn->getUuid());
        const auto &slotAParent = slotA->getParentComponent();
        const auto &slotBParent = slotB->getParentComponent();

        sceneState.removeConnectionForComponent(slotAParent, conn->getUuid());
        sceneState.removeConnectionForComponent(slotBParent, conn->getUuid());

        removedIds.push_back(conn->getUuid());

        return removedIds;
    }

    bool SvcConnection::isSlotRemovable(const std::shared_ptr<Canvas::SlotSceneComponent> &slot,
                                        size_t connectionThreshold) {
        if (!slot) {
            BESS_ERROR("[SvcConnection] [isSlotRemovable] Invalid slot given");
            return false;
        }

        const size_t idx = slot->getIndex();

        // False: if there are still some connections or is the first or only slot or
        // if its not a slot (its a joint)
        if (slot->getConnectedConnections().size() > connectionThreshold ||
            idx == 0 ||
            slot->getType() != Canvas::SceneComponentType::slot)
            return false;

        const auto &sceneState = getScene()->getState();
        const auto &parentId = slot->getParentComponent();
        const auto &parent = sceneState.getComponentByUuid<Canvas::SimulationSceneComponent>(parentId);

        // False: if parent is not valid
        if (!parent) {
            BESS_ERROR("Parent component {} of slot {} not valid",
                       (uint64_t)parentId, (uint64_t)slot->getUuid());
            BESS_ASSERT(false, "Parent component of slot not valid");
            return false;
        }

        const auto &simEngine = getSimEngine();
        const auto &def = simEngine.getComponentDefinition(parent->getSimEngineId());

        const bool isInput = slot->getSlotType() == Canvas::SlotType::digitalInput;

        const SimEngine::SlotsGroupInfo &slotsInfo = isInput
                                                         ? def->getInputSlotsInfo()
                                                         : def->getOutputSlotsInfo();

        // False: if slots group is not resizeable
        if (!slotsInfo.isResizeable)
            return false;

        // True: only if its the last slot in the component
        return (idx + 1) == slotsInfo.count;
    }

    bool SvcConnection::removeSlot(const std::shared_ptr<Canvas::SlotSceneComponent> &slot) {
        if (!slot) {
            BESS_ERROR("[SvcConnection] [removeSlot] Invalid slot given");
            return false;
        }

        if (!isSlotRemovable(slot)) {
            BESS_WARN("Cannot remove slot {} because it's not removable", (uint64_t)slot->getUuid());
            return false;
        }

        const bool isInput = slot->getSlotType() == Canvas::SlotType::digitalInput;

        auto &sceneState = getScene()->getState();
        const auto &parentId = slot->getParentComponent();
        const auto &parent = sceneState.getComponentByUuid<Canvas::SimulationSceneComponent>(parentId);

        const auto &simEngine = getSimEngine();
        const auto &digComp = simEngine.getDigitalComponent(parent->getSimEngineId());
        const auto &def = digComp->definition;

        const SimEngine::SlotsGroupInfo &slotsInfo = isInput
                                                         ? def->getInputSlotsInfo()
                                                         : def->getOutputSlotsInfo();

        auto const prevCount = slotsInfo.count;
        size_t newCount = 0;

        if (isInput) {
            newCount = digComp->decrementInputCount();
        } else {
            newCount = digComp->decrementOutputCount();
        }

        if (newCount != prevCount - 1) {
            BESS_ERROR("Failed to decrement slot count for component {}, isInput = {}, prevCount = {}, newCount = {}",
                       (uint64_t)parent->getSimEngineId(), isInput, prevCount, newCount);
            BESS_ASSERT(false, "Failed to decrement slot count for component");
            return false;
        }

        auto &slots = isInput ? parent->getInputSlots() : parent->getOutputSlots();

        // remove from io slots container
        slots.erase(std::ranges::remove(slots, slot->getUuid()).begin(),
                    slots.end());

        // remove from parent child entry
        parent->removeChildComponent(slot->getUuid());

        // remove from scene
        sceneState.removeComponent(slot->getUuid(), UUID::master);

        return true;
    }

    bool SvcConnection::addSlot(const std::shared_ptr<Canvas::SlotSceneComponent> &slot) {
        if (!slot) {
            BESS_ERROR("[SvcConnection] [addSlot] Invalid slot given");
            return false;
        }

        auto &sceneState = getScene()->getState();
        const auto &parentId = slot->getParentComponent();
        const auto &parent = sceneState.getComponentByUuid<Canvas::SimulationSceneComponent>(parentId);

        if (!parent) {
            BESS_ERROR("Parent component {} of slot {} not valid",
                       (uint64_t)parentId, (uint64_t)slot->getUuid());
            BESS_ASSERT(false, "Parent component of slot not valid");
            return false;
        }

        const auto &simEngine = getSimEngine();
        const auto &digComp = simEngine.getDigitalComponent(parent->getSimEngineId());
        const auto &def = digComp->definition;

        const bool isInput = slot->getSlotType() == Canvas::SlotType::digitalInput;

        const SimEngine::SlotsGroupInfo &slotsInfo = isInput
                                                         ? def->getInputSlotsInfo()
                                                         : def->getOutputSlotsInfo();

        if (!slotsInfo.isResizeable) {
            BESS_WARN("Cannot add slot to component {} because its {} slots are not resizeable",
                      (uint64_t)parent->getSimEngineId(), isInput ? "input" : "output");

            BESS_ASSERT(false, "Trying to add slot to non-resizeable slots group");
            return false;
        }

        const auto slotsCount = slotsInfo.count;

        if (isInput) {
            const auto newCount = digComp->incrementInputCount();
            if (newCount != slotsCount + 1) {
                BESS_ERROR("Failed to increment input slot count for component {}, prevCount = {}, newCount = {}",
                           (uint64_t)parent->getSimEngineId(), slotsCount, newCount);
                BESS_ASSERT(false, "Failed to increment input slot count for component");
                return false;
            }
        } else {
            const auto newCount = digComp->incrementOutputCount();
            if (newCount != slotsCount + 1) {
                BESS_ERROR("Failed to increment output slot count for component {}, prevCount = {}, newCount = {}",
                           (uint64_t)parent->getSimEngineId(), slotsCount, newCount);
                BESS_ASSERT(false, "Failed to increment output slot count for component");
                return false;
            }
        }

        if (isInput) {
            parent->addInputSlot(slot->getUuid(), true);
        } else {
            parent->addOutputSlot(slot->getUuid(), true);
        }

        parent->addChildComponent(slot->getUuid());

        sceneState.addComponent(slot);

        return true;
    }

    bool SvcConnection::isResizeTriggerSlot(const std::shared_ptr<Canvas::SlotSceneComponent> &slot) {
        const auto type = slot->getSlotType();
        return type == Canvas::SlotType::inputsResize ||
               type == Canvas::SlotType::outputsResize;
    }

    std::shared_ptr<Canvas::Scene> SvcConnection::getScene() {
        auto &page = Pages::MainPage::getInstance();
        return page->getState().getSceneDriver().getActiveScene();
    }

    SimEngine::SimulationEngine &SvcConnection::getSimEngine() {
        return SimEngine::SimulationEngine::instance();
    }

    std::pair<std::shared_ptr<Canvas::SlotSceneComponent>, bool> SvcConnection::tryFindSlot(const UUID &slotId) {
        const auto &sceneState = getScene()->getState();

        // try to find in scene
        const auto &slot = sceneState.getComponentByUuid<Canvas::SlotSceneComponent>(slotId);

        if (slot) {
            return {slot, true};
        }

        // then from internal bin
        if (m_slotsBin.contains(slotId)) {
            auto slot = m_slotsBin.at(slotId);
            BESS_DEBUG("Found slot {} in bin", (uint64_t)slotId);
            m_slotsBin.erase(slotId);
            return {slot, false};
        }

        return {nullptr, false};
    }

    std::optional<std::string> SvcConnection::connectInSimEngine(const UUID &idA, const UUID &idB) {
        auto &simEngine = getSimEngine();
        const auto &sceneState = getScene()->getState();

        const auto &slotCompA = getSlot(idA);
        const auto &slotCompB = getSlot(idB);

        if (!slotCompA) {
            BESS_ERROR("Failed to get slot component A with id {} for connection", (uint64_t)idA);
            BESS_ASSERT(false, "Failed to get slot component A for connection");
            return "Failed to get slot component A for connection";
        }

        if (!slotCompB) {
            BESS_ERROR("Failed to get slot component B with id {} for connection", (uint64_t)idB);
            BESS_ASSERT(false, "Failed to get slot component B for connection");
            return "Failed to get slot component B for connection";
        }

        const auto &simCompA = sceneState.getComponentByUuid<Canvas::SimulationSceneComponent>(
            slotCompA->getParentComponent());
        const auto &simCompB = sceneState.getComponentByUuid<Canvas::SimulationSceneComponent>(
            slotCompB->getParentComponent());

        const auto pinTypeA = slotCompA->getSlotType() == Canvas::SlotType::digitalInput
                                  ? SimEngine::SlotType::digitalInput
                                  : SimEngine::SlotType::digitalOutput;
        const auto pinTypeB = slotCompB->getSlotType() == Canvas::SlotType::digitalInput
                                  ? SimEngine::SlotType::digitalInput
                                  : SimEngine::SlotType::digitalOutput;

        const auto success = simEngine.connectComponent(simCompA->getSimEngineId(), slotCompA->getIndex(), pinTypeA,
                                                        simCompB->getSimEngineId(), slotCompB->getIndex(), pinTypeB);

        if (!success) {
            BESS_WARN("[ConnectionSvc] Failed to connect slots in simulation engine between component {} slot {} and component {} slot {}",
                      (uint64_t)simCompA->getUuid(), slotCompA->getIndex(),
                      (uint64_t)simCompB->getUuid(), slotCompB->getIndex());
            return "Failed to connect slots in simulation engine";
        }

        return std::nullopt;
    }

    bool SvcConnection::disconnectInSimEngine(const UUID &idA, const UUID &idB) {
        auto &simEngine = getSimEngine();
        const auto &sceneState = getScene()->getState();

        const auto &slotCompA = getSlot(idA);
        const auto &slotCompB = getSlot(idB);

        if (!slotCompA) {
            BESS_ERROR("Failed to get slot component A with id {} for disconnection", (uint64_t)idA);
            BESS_ASSERT(false, "Failed to get slot component A for disconnection");
            return false;
        }

        if (!slotCompB) {
            BESS_ERROR("Failed to get slot component B with id {} for disconnection", (uint64_t)idB);
            BESS_ASSERT(false, "Failed to get slot component B for disconnection");
            return false;
        }

        const auto &simCompA = sceneState.getComponentByUuid<Canvas::SimulationSceneComponent>(
            slotCompA->getParentComponent());
        const auto &simCompB = sceneState.getComponentByUuid<Canvas::SimulationSceneComponent>(
            slotCompB->getParentComponent());

        const auto pinTypeA = slotCompA->getSlotType() == Canvas::SlotType::digitalInput
                                  ? SimEngine::SlotType::digitalInput
                                  : SimEngine::SlotType::digitalOutput;
        const auto pinTypeB = slotCompB->getSlotType() == Canvas::SlotType::digitalInput
                                  ? SimEngine::SlotType::digitalInput
                                  : SimEngine::SlotType::digitalOutput;

        simEngine.deleteConnection(simCompA->getSimEngineId(), pinTypeA, slotCompA->getIndex(),
                                   simCompB->getSimEngineId(), pinTypeB, slotCompB->getIndex());

        return true;
    }

    std::shared_ptr<Canvas::SlotSceneComponent> SvcConnection::getSlot(const UUID &compId) {
        const auto &sceneState = getScene()->getState();

        auto comp = sceneState.getComponentByUuid(compId);

        if (!comp) {
            BESS_ERROR("Component with id {} not found in scene", (uint64_t)compId);
            BESS_ASSERT(false, "Component not found in scene");
            return nullptr;
        }

        return comp->cast<Canvas::SlotSceneComponent>();
    }

    std::optional<std::string> SvcConnection::connectProxySlots(const UUID &proxyA, const UUID &proxyB) {
        BESS_ASSERT(false, "connectProxySlots not implemented yet");
    }

    std::optional<std::string> SvcConnection::connectSlotToProxy(const UUID &slotId, const UUID &proxyId) {
        const auto &sceneState = getScene()->getState();

        const auto &slotComp = getSlot(slotId);
        if (!slotComp) {
            BESS_ERROR("Failed to get slot component with id {} for connecting to proxy", (uint64_t)slotId);
            BESS_ASSERT(false, "Failed to get slot component for connecting to proxy");
            return "Failed to get slot component for connecting to proxy";
        }

        const auto &proxyComp = sceneState.getComponentByUuid(proxyId);
        auto proxySlot = std::dynamic_pointer_cast<Canvas::ProxySlotComponent>(proxyComp);

        if (!proxySlot) {
            BESS_ERROR("Failed to get proxy slot component with id {} for connecting to proxy", (uint64_t)proxyId);
            BESS_ASSERT(false, "Failed to get proxy slot component for connecting to proxy");
            return "Failed to get proxy slot component for connecting to proxy";
        }

        UUID actualSlotId = UUID::null;

        if (slotComp->getSlotType() == Canvas::SlotType::digitalInput) {
            actualSlotId = proxySlot->getOutputSlotId();
        } else {
            actualSlotId = proxySlot->getInputSlotId();
        }

        const auto res = connectSlots(slotId, actualSlotId);

        if (res.has_value()) {
            BESS_ERROR("Failed to connect slot with id {} to proxy slot with id {} in sim engine, error: {}",
                       (uint64_t)slotId, (uint64_t)actualSlotId, res.value());
            BESS_ASSERT(false, "Failed to connect slot to proxy slot in sim engine");
            return "Failed to connect slot to proxy slot in sim engine";
        }

        // Update all the connections from this proxy
        for (const auto &connId : proxySlot->getConnections()) {
            const auto &connComp = sceneState.getComponentByUuid<
                Canvas::ConnectionSceneComponent>(connId);

            if (!connComp) {
                continue;
            }

            const auto slotAId = connComp->getStartSlot();
            const auto slotBId = connComp->getEndSlot();

            Bess::UUID potentialId = slotAId;
            if (slotAId == actualSlotId) {
                potentialId = slotBId;
            }

            connect(potentialId, slotId);
        }

        return std::nullopt;
    }

    std::optional<std::string> SvcConnection::connectSlots(const UUID &slotAId, const UUID &slotBId) {
        return connectInSimEngine(slotAId, slotBId);
    }

    std::optional<std::string> SvcConnection::connect(const UUID &idA, const UUID &idB) {
        const auto &sceneState = getScene()->getState();

        // figure out types of two and call the correct connect function
        const auto &compA = sceneState.getComponentByUuid(idA);
        const auto &compB = sceneState.getComponentByUuid(idB);

        if (!compA) {
            BESS_ERROR("Component A with id {} not found in scene for connecting", (uint64_t)idA);
            BESS_ASSERT(false, "Component A not found in scene for connecting");
            return "Component A not found in scene for connecting";
        }

        if (!compB) {
            BESS_ERROR("Component B with id {} not found in scene for connecting", (uint64_t)idB);
            BESS_ASSERT(false, "Component B not found in scene for connecting");
            return "Component B not found in scene for connecting";
        }

        const auto &typeA = compA->getType();
        const auto &typeB = compB->getType();

        // Both are slots
        if (typeA == Canvas::SceneComponentType::slot &&
            typeB == Canvas::SceneComponentType::slot) {
            return connectSlots(idA, idB);
        }

        // Type A is slot then assume slot B is proxy slot
        if (typeA == Canvas::SceneComponentType::slot) {
            const auto &proxySlot = std::dynamic_pointer_cast<Canvas::ProxySlotComponent>(compB);
            if (!proxySlot) {
                BESS_ERROR("Component B with id {} is not a proxy slot component", (uint64_t)idB);
                BESS_ASSERT(false, "Component B is not a proxy slot component");
                return "Component B is not a proxy slot component";
            }

            return connectSlotToProxy(idA, idB);
        }

        // Type B is slot then assume slot A is proxy slot
        if (typeB == Canvas::SceneComponentType::slot) {
            const auto &proxySlot = std::dynamic_pointer_cast<Canvas::ProxySlotComponent>(compA);
            if (!proxySlot) {
                BESS_ERROR("Component A with id {} is not a proxy slot component", (uint64_t)idA);
                BESS_ASSERT(false, "Component A is not a proxy slot component");
                return "Component A is not a proxy slot component";
            }

            return connectSlotToProxy(idB, idA);
        }

        // Both are proxy slots
        const auto &proxySlotA = std::dynamic_pointer_cast<Canvas::ProxySlotComponent>(compA);
        const auto &proxySlotB = std::dynamic_pointer_cast<Canvas::ProxySlotComponent>(compB);

        if (!proxySlotA) {
            BESS_ERROR("Component A with id {} is not a proxy slot component", (uint64_t)idA);
            BESS_ASSERT(false, "Component A is not a proxy slot component");
            return "Component A is not a proxy slot component";
        }

        if (!proxySlotB) {
            BESS_ERROR("Component B with id {} is not a proxy slot component", (uint64_t)idB);
            BESS_ASSERT(false, "Component B is not a proxy slot component");
            return "Component B is not a proxy slot component";
        }

        return connectProxySlots(idA, idB);
    }

    std::optional<std::string> SvcConnection::regConnToComp(const UUID &compId,
                                                            const UUID &connId) {

        auto &sceneState = getScene()->getState();

        const auto &comp = sceneState.getComponentByUuid(compId);

        if (!comp) {
            BESS_ERROR("Component with id {} not found in scene for registering connection", (uint64_t)compId);
            BESS_ASSERT(false, "Component not found in scene for registering connection");
            return "Component not found in scene for registering connection";
        }

        if (comp->getType() == Canvas::SceneComponentType::slot) {
            const auto &slotComp = comp->cast<Canvas::SlotSceneComponent>();
            slotComp->addConnection(connId);
        } else {
            const auto &proxy = std::dynamic_pointer_cast<Canvas::ProxySlotComponent>(comp);
            if (!proxy) {
                BESS_ERROR("Component with id {} is not a proxy slot component for registering connection",
                           (uint64_t)compId);
                BESS_ASSERT(false, "Component is not a proxy slot component for registering connection");
                return "Component is not a proxy slot component for registering connection";
            }
            proxy->addConnection(connId);
        }

        return std::nullopt;
    }
} // namespace Bess::Svc
