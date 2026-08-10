#include "pages/main_page/services/connection_service.h"
#include "common/bess_assert.h"
#include "common/bess_uuid.h"
#include "common/logger.h"
#include "dig_sim_driver.h"

#include "bess_core/scene/scene.h"
#include "pages/main_page/scene_components/connection_scene_component.h"
#include "pages/main_page/scene_components/proxy_slot_component.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "simulation_engine.h"
#include <algorithm>
#include <cstdint>
#include <optional>

namespace Bess::Svc {
    namespace {
        SimEngine::PortDirection realPortDirectionFor(bool isInput) {
            return isInput ? SimEngine::PortDirection::input
                           : SimEngine::PortDirection::output;
        }

        bool isRealSlotForSide(const Canvas::SlotSceneComponent &slot,
                               bool isInput) {
            return !slot.isResizeSlot() &&
                   slot.getPortDirection() == realPortDirectionFor(isInput);
        }

        void configureSlotPort(
            const std::shared_ptr<Canvas::SlotSceneComponent> &slot,
            bool isInput,
            bool resizeTrigger,
            SimEngine::SignalKind signalKind) {
            if (!slot) {
                return;
            }

            slot->setPortDirection(realPortDirectionFor(isInput));
            slot->setSignalKind(signalKind);
            slot->setResizeTrigger(resizeTrigger);
        }

        size_t realSlotEnd(const Canvas::SceneState &sceneState,
                           const std::vector<UUID> &slotIds) {
            if (slotIds.empty()) {
                return 0;
            }

            const auto tail =
                sceneState.getComponentByUuid<Canvas::SlotSceneComponent>(
                    slotIds.back());
            return tail && tail->isResizeSlot() ? slotIds.size() - 1
                                                : slotIds.size();
        }

        size_t restoredSlotIndex(const Canvas::SceneState &sceneState,
                                 const std::vector<UUID> &slotIds,
                                 int savedIndex) {
            const auto concreteEnd = realSlotEnd(sceneState, slotIds);
            if (savedIndex < 0) {
                return concreteEnd;
            }

            return std::min(static_cast<size_t>(savedIndex), concreteEnd);
        }

        std::optional<SimEngine::PortDescriptor>
        portDescriptorFor(SimEngine::SimulationEngine &simEngine,
                          const Canvas::SimulationSceneComponent &parent,
                          bool isInput) {
            const auto &def =
                simEngine.getComponentDefinition(parent.getSimEngineId());
            if (!def) {
                return std::nullopt;
            }

            return isInput ? def->getInputPortDescriptor()
                           : def->getOutputPortDescriptor();
        }

        void reindexRealSlots(const Canvas::SceneState &sceneState,
                              const std::vector<UUID> &slotIds) {
            int nextIndex = 0;
            for (const auto &slotId : slotIds) {
                const auto slot =
                    sceneState.getComponentByUuid<Canvas::SlotSceneComponent>(
                        slotId);
                if (!slot || slot->isResizeSlot()) {
                    continue;
                }

                slot->setIndex(nextIndex++);
            }
        }

        std::shared_ptr<Canvas::SlotSceneComponent>
        findRealSlotAtIndex(const Canvas::SceneState &sceneState,
                            const std::vector<UUID> &slotIds,
                            bool isInput,
                            int index) {
            for (const auto &slotId : slotIds) {
                const auto slot =
                    sceneState.getComponentByUuidSP<Canvas::SlotSceneComponent>(
                        slotId);
                if (slot && isRealSlotForSide(*slot, isInput) &&
                    slot->getIndex() == index) {
                    return slot;
                }
            }

            return nullptr;
        }
    } // namespace

    void SvcConnection::onInit() {
        m_slotsBin = {};
        BESS_DEBUG("Initialized Connection Service");
    }

    void SvcConnection::onDestroy() {
        m_slotsBin.clear();
        m_sim = nullptr;
        BESS_DEBUG("Destroyed Connection Service");
    }

    void SvcConnection::setSimEngine(SimEngine::SimulationEngine *sim) {
        m_sim = sim;
    }

    std::shared_ptr<Canvas::ConnectionSceneComponent>
    SvcConnection::createConnection(
        const UUID &slotAId,
        const UUID &slotBId,
        const std::shared_ptr<Canvas::Scene> &scene) {
        auto conn = std::make_shared<Canvas::ConnectionSceneComponent>();
        conn->setStartEndSlots(slotAId, slotBId);
        if (!addConnection(conn, scene)) {
            return nullptr;
        }
        return conn;
    }

    std::shared_ptr<Canvas::ConnectionSceneComponent>
    SvcConnection::createConnection(
        const Bess::UUID &fromCompId,
        Bess::SimEngine::PortDirection fromDirection,
        int fromPortIdx,
        const Bess::UUID &toCompId,
        Bess::SimEngine::PortDirection toDirection,
        int toPortIdx,
        const std::shared_ptr<Canvas::Scene> &scene,
        Bess::SimEngine::SignalKind signalKind) {
        (void)signalKind;
        if (!scene) {
            BESS_ERROR("[SvcConnection] Cannot create connection without a "
                       "scene");
            return nullptr;
        }

        auto &sceneState = scene->getState();

        const auto &fromComp =
            sceneState.getComponentByUuid<Canvas::SimulationSceneComponent>(
                fromCompId);
        const auto &toComp =
            sceneState.getComponentByUuid<Canvas::SimulationSceneComponent>(
                toCompId);

        if (!fromComp) {
            BESS_ERROR("From component with id {} not found in scene state",
                       (uint64_t)fromCompId);
            return nullptr;
        }

        if (!toComp) {
            BESS_ERROR("To component with id {} not found in scene state",
                       (uint64_t)toCompId);
            return nullptr;
        }

        UUID fromSlotId, toSlotId;

        if (fromDirection == SimEngine::PortDirection::input) {
            fromSlotId = fromComp->getInputSlots().at(fromPortIdx);
        } else {
            fromSlotId = fromComp->getOutputSlots().at(fromPortIdx);
        }

        if (toDirection == SimEngine::PortDirection::input) {
            toSlotId = toComp->getInputSlots().at(toPortIdx);
        } else {
            toSlotId = toComp->getOutputSlots().at(toPortIdx);
        }

        return createConnection(fromSlotId, toSlotId, scene);
    }

    std::vector<UUID>
    SvcConnection::getDependants(const UUID &connection,
                                 const std::shared_ptr<Canvas::Scene> &scene) {
        if (!scene) {
            BESS_ERROR("[SvcConnection] Cannot get dependants without a scene");
            return {};
        }

        auto &sceneState = scene->getState();
        if (!sceneState.isComponentValid(connection)) {
            BESS_ERROR("Connection with id {} not found in scene state",
                       (uint64_t)connection);
            return {};
        }

        const auto &connComp =
            sceneState.getComponentByUuid<Canvas::ConnectionSceneComponent>(
                connection);

        // Check for slots which can be removed with the connection
        const auto &slotA = getSlot(scene, connComp->getStartSlot());
        const auto &slotB = getSlot(scene, connComp->getEndSlot());

        std::vector<UUID> dependants;

        if (slotA && isSlotRemovable(scene, slotA, 1)) {
            dependants.push_back(slotA->getUuid());
        }

        if (slotB && isSlotRemovable(scene, slotB, 1)) {
            dependants.push_back(slotB->getUuid());
        }

        return dependants;
    }

    /// TODO (shivang): handel connecting to proxys, correctly,
    /// if start was a output slot then take input slot of proxy and connect to
    /// it, and vice versa. correctly add connection to the proxy component
    bool SvcConnection::addConnection(
        const std::shared_ptr<Canvas::ConnectionSceneComponent> &conn,
        const std::shared_ptr<Canvas::Scene> &scene) {
        if (!conn) {
            BESS_ERROR(
                "[SvcConnection] [addConnection] Invalid connection given");
            return false;
        }

        if (!scene) {
            BESS_ERROR("[SvcConnection] [addConnection] Invalid scene given");
            return false;
        }

        BESS_DEBUG("Adding connection with uuid {} between slot {} and slot {}",
                   (uint64_t)conn->getUuid(),
                   (uint64_t)conn->getStartSlot(),
                   (uint64_t)conn->getEndSlot());

        auto &sceneState = scene->getState();

        auto slotAId = conn->getStartSlot();
        auto slotBId = conn->getEndSlot();

        auto [slotA, foundAInScene] = tryFindSlot(scene, slotAId);
        auto [slotB, foundBInScene] = tryFindSlot(scene, slotBId);

        const auto isSlotRegistered =
            [&](const std::shared_ptr<Canvas::SlotSceneComponent> &slot) {
                if (!slot || slot->isResizeSlot()) {
                    return true;
                }

                const auto parent =
                    sceneState
                        .getComponentByUuid<Canvas::SimulationSceneComponent>(
                            slot->getParentComponent());
                if (!parent || parent->getSimEngineId() == UUID::null) {
                    return false;
                }

                const auto &slots = slot->isInputSlot()
                                        ? parent->getInputSlots()
                                        : parent->getOutputSlots();
                if (!std::ranges::contains(slots, slot->getUuid())) {
                    return false;
                }

                if (slot->getIndex() < 0) {
                    return false;
                }

                const auto descriptor = portDescriptorFor(
                    getSimEngine(), *parent, slot->isInputSlot());
                if (!descriptor) {
                    return false;
                }

                return descriptor->signalKind == slot->getSignalKind() &&
                       static_cast<size_t>(slot->getIndex()) <
                           descriptor->count;
            };

        const auto endpointA = sceneState.getComponentByUuid(slotAId);
        const auto endpointB = sceneState.getComponentByUuid(slotBId);
        const bool endpointAIsProxy =
            endpointA &&
            endpointA->getType() != Canvas::SceneComponentType::slot &&
            (dynamic_cast<Canvas::ProxySlotComponent *>(endpointA) != nullptr);
        const bool endpointBIsProxy =
            endpointB &&
            endpointB->getType() != Canvas::SceneComponentType::slot &&
            (dynamic_cast<Canvas::ProxySlotComponent *>(endpointB) != nullptr);

        if (!slotA && !endpointAIsProxy) {

            BESS_ERROR("Slot A with id {} of connection {} not found",
                       (uint64_t)slotAId,
                       (uint64_t)conn->getUuid());
            BESS_ASSERT(false, "Slot A of the connection not found");
            return false;
        }

        if (!slotB && !endpointBIsProxy) {
            BESS_ERROR("Slot B with id {} of connection {} not found",
                       (uint64_t)slotBId,
                       (uint64_t)conn->getUuid());
            BESS_ASSERT(false, "Slot B of the connection not found");
            return false;
        }

        if (slotA && foundAInScene && isResizeTriggerSlot(slotA)) {
            slotA = createSlotFromResizeTrigger(scene, slotA);
            if (!slotA) {
                BESS_ERROR("Failed to create slot from resize trigger A for "
                           "connection {}",
                           (uint64_t)conn->getUuid());
                return false;
            }
            slotAId = slotA->getUuid();
            conn->setStartEndSlots(slotAId, slotBId);
            foundAInScene = true; // The new slot was added to the scene in
                                  // createSlotFromResizeTrigger
        }

        if (slotA && (!foundAInScene || !isSlotRegistered(slotA)) &&
            !addSlot(scene, slotA)) {
            BESS_ERROR("Failed to add slot A with id {} for connection {}",
                       (uint64_t)slotAId,
                       (uint64_t)conn->getUuid());
            BESS_ASSERT(false, "Failed to add slot A for connection");
            return false;
        }

        if (slotB && foundBInScene && isResizeTriggerSlot(slotB)) {
            slotB = createSlotFromResizeTrigger(scene, slotB);
            if (!slotB) {
                BESS_ERROR("Failed to create slot from resize trigger B for "
                           "connection {}",
                           (uint64_t)conn->getUuid());
                return false;
            }
            slotBId = slotB->getUuid();
            conn->setStartEndSlots(slotAId, slotBId);
            foundBInScene = true; // The new slot was added to the scene in
                                  // createSlotFromResizeTrigger
        }

        if (slotB && (!foundBInScene || !isSlotRegistered(slotB)) &&
            !addSlot(scene, slotB)) {
            BESS_ERROR("Failed to add slot B with id {} for connection {}",
                       (uint64_t)slotBId,
                       (uint64_t)conn->getUuid());
            BESS_ASSERT(false, "Failed to add slot B for connection");
            return false;
        }

        const auto &res = connect(scene, slotAId, slotBId);

        if (res.has_value()) {
            BESS_ERROR("Failed to connect slots {} and {} in sim engine for "
                       "connection {}, error: {}",
                       (uint64_t)slotAId,
                       (uint64_t)slotBId,
                       (uint64_t)conn->getUuid(),
                       res.value());
            // BESS_ASSERT(false, "Failed to connect slots in sim engine for
            // connection");
            return false;
        }

        regConnToComp(scene, slotAId, conn->getUuid());
        regConnToComp(scene, slotBId, conn->getUuid());

        sceneState.addComponent(conn);

        BESS_INFO(
            "[ConnectionSvc] Added connection {} between slot {} and slot {}",
            (uint64_t)conn->getUuid(),
            (uint64_t)slotAId,
            (uint64_t)slotBId);

        return true;
    }

    std::vector<UUID> SvcConnection::removeConnection(
        const std::shared_ptr<Canvas::ConnectionSceneComponent> &conn,
        const std::shared_ptr<Canvas::Scene> &scene) {

        if (!conn) {
            BESS_ERROR(
                "[SvcConnection] [removeConnection] Invalid connection given");
            return {};
        }

        if (!scene) {
            BESS_ERROR(
                "[SvcConnection] [removeConnection] Invalid scene given");
            return {};
        }

        auto &sceneState = scene->getState();

        const auto &startSlotId = conn->getStartSlot();
        const auto &endSlotId = conn->getEndSlot();

        auto [slotA, slotB] =
            resolvePhysicalSlotPair(scene, startSlotId, endSlotId);

        if (!slotA || !slotB) {
            BESS_ERROR("Failed to get fully resolved slot components for "
                       "disconnection.");
            return {};
        }

        BESS_ASSERT(slotA,
                    "Failed to get resolved slot A for connection removal");
        BESS_ASSERT(slotB,
                    "Failed to get resolved slot B for connection removal");

        const auto &slotAId = slotA->getUuid();
        const auto &slotBId = slotB->getUuid();

        disconnect(scene, startSlotId, endSlotId);

        auto removedIds = std::vector<UUID>{};
        const auto findPairedSlotRemovedWith =
            [&](const std::shared_ptr<Canvas::SlotSceneComponent> &slot)
            -> std::shared_ptr<Canvas::SlotSceneComponent> {
            if (!slot || slot->getIndex() < 0) {
                return nullptr;
            }

            const auto parent =
                sceneState.getComponentByUuid<Canvas::SimulationSceneComponent>(
                    slot->getParentComponent());
            if (!parent) {
                return nullptr;
            }

            const auto digComp =
                getSimEngine()
                    .getComponent<SimEngine::Drivers::Digital::DigSimComp>(
                        parent->getSimEngineId());
            if (!digComp) {
                return nullptr;
            }

            const auto digDef =
                digComp
                    ->getDefinition<SimEngine::Drivers::Digital::DigCompDef>();
            if (!digDef || !digDef->getKeepIOCountEq()) {
                return nullptr;
            }

            const auto &pairedSlots = slot->isInputSlot()
                                          ? parent->getOutputSlots()
                                          : parent->getInputSlots();
            return findRealSlotAtIndex(sceneState,
                                       pairedSlots,
                                       !slot->isInputSlot(),
                                       slot->getIndex());
        };

        // Processing Slot-A
        {
            slotA->removeConnection(conn->getUuid());
            if (isSlotRemovable(scene, slotA)) {
                const auto pairedSlot = findPairedSlotRemovedWith(slotA);
                m_slotsBin[slotA->getUuid()] = slotA;
                if (!removeSlot(scene, slotA)) {
                    BESS_ERROR(
                        "Failed to remove slot A with id {} for connection {}",
                        (uint64_t)slotAId,
                        (uint64_t)conn->getUuid());
                    BESS_ASSERT(false,
                                "Failed to remove slot A for connection");
                    return {};
                }
                removedIds.push_back(slotA->getUuid());
                if (pairedSlot) {
                    removedIds.push_back(pairedSlot->getUuid());
                }
            }
        }

        // Processing Slot-B
        {
            slotB->removeConnection(conn->getUuid());
            if (isSlotRemovable(scene, slotB)) {
                const auto pairedSlot = findPairedSlotRemovedWith(slotB);
                m_slotsBin[slotB->getUuid()] = slotB;
                if (!removeSlot(scene, slotB)) {
                    BESS_ERROR(
                        "Failed to remove slot B with id {} for connection {}",
                        (uint64_t)slotBId,
                        (uint64_t)conn->getUuid());
                    BESS_ASSERT(false,
                                "Failed to remove slot B for connection");
                    return {};
                }
                removedIds.push_back(slotB->getUuid());
                if (pairedSlot) {
                    removedIds.push_back(pairedSlot->getUuid());
                }
            }
        }

        sceneState.removeComponent(conn->getUuid());

        // Inform proxies if involved
        auto startComp = sceneState.getComponentByUuid(startSlotId);
        if (startComp &&
            startComp->getType() != Canvas::SceneComponentType::slot) {
            auto proxyA = dynamic_cast<Canvas::ProxySlotComponent *>(startComp);
            if (proxyA)
                proxyA->removeConnection(conn->getUuid());
        }

        auto endComp = sceneState.getComponentByUuid(endSlotId);
        if (endComp && endComp->getType() != Canvas::SceneComponentType::slot) {
            auto proxyB = dynamic_cast<Canvas::ProxySlotComponent *>(endComp);
            if (proxyB)
                proxyB->removeConnection(conn->getUuid());
        }

        removedIds.push_back(conn->getUuid());

        return removedIds;
    }

    bool SvcConnection::isSlotRemovable(
        const std::shared_ptr<Canvas::Scene> &scene,
        const std::shared_ptr<Canvas::SlotSceneComponent> &slot,
        size_t connectionThreshold) {
        if (!scene) {
            BESS_ERROR("[SvcConnection] [isSlotRemovable] Invalid scene given");
            return false;
        }

        if (!slot) {
            BESS_ERROR("[SvcConnection] [isSlotRemovable] Invalid slot given");
            return false;
        }

        if (slot->getType() != Canvas::SceneComponentType::slot ||
            slot->isResizeSlot()) {
            return false;
        }

        if (slot->getConnectedConnections().size() > connectionThreshold ||
            slot->getIndex() <= 0) {
            return false;
        }

        const auto &sceneState = scene->getState();
        const auto parent =
            sceneState.getComponentByUuid<Canvas::SimulationSceneComponent>(
                slot->getParentComponent());

        if (!parent) {
            return false;
        }

        const bool isInput = slot->isInputSlot();
        const auto slotsInfo =
            portDescriptorFor(getSimEngine(), *parent, isInput);
        if (!slotsInfo) {
            return false;
        }

        if (!slotsInfo->isResizeable) {
            return false;
        }

        return (slot->getIndex() + 1) == slotsInfo->count;
    }

    bool SvcConnection::removeSlot(
        const std::shared_ptr<Canvas::Scene> &scene,
        const std::shared_ptr<Canvas::SlotSceneComponent> &slot) {
        if (!scene) {
            BESS_ERROR("[SvcConnection] [removeSlot] Invalid scene given");
            return false;
        }

        if (!slot) {
            BESS_ERROR("[SvcConnection] [removeSlot] Invalid slot given");
            return false;
        }

        if (!isSlotRemovable(scene, slot)) {
            return false;
        }

        auto &sceneState = scene->getState();
        const auto parent =
            sceneState.getComponentByUuid<Canvas::SimulationSceneComponent>(
                slot->getParentComponent());
        if (!parent) {
            return false;
        }

        const bool isInput = slot->isInputSlot();
        const int removedIndex = slot->getIndex();
        if (removedIndex < 0) {
            return false;
        }

        const auto &slots =
            isInput ? parent->getInputSlots() : parent->getOutputSlots();
        const auto &pairedSlots =
            isInput ? parent->getOutputSlots() : parent->getInputSlots();

        std::shared_ptr<Canvas::SlotSceneComponent> pairedSlot;
        const auto digComp =
            getSimEngine()
                .getComponent<SimEngine::Drivers::Digital::DigSimComp>(
                    parent->getSimEngineId());
        if (digComp) {
            const auto digDef =
                digComp
                    ->getDefinition<SimEngine::Drivers::Digital::DigCompDef>();
            if (digDef && digDef->getKeepIOCountEq()) {
                pairedSlot = findRealSlotAtIndex(
                    sceneState, pairedSlots, !isInput, removedIndex);
            }
        }

        if (isInput) {
            parent->removeInputSlot(slot->getUuid());
        } else {
            parent->removeOutputSlot(slot->getUuid());
        }

        parent->removeChildComponent(slot->getUuid());
        sceneState.removeComponent(slot->getUuid(), UUID::master);

        if (pairedSlot) {
            m_slotsBin[pairedSlot->getUuid()] = pairedSlot;
            if (isInput) {
                parent->removeOutputSlot(pairedSlot->getUuid());
            } else {
                parent->removeInputSlot(pairedSlot->getUuid());
            }
            parent->removeChildComponent(pairedSlot->getUuid());
            if (sceneState.isComponentValid(pairedSlot->getUuid())) {
                sceneState.removeComponent(pairedSlot->getUuid(), UUID::master);
            }
            reindexRealSlots(sceneState, pairedSlots);
        }

        getSimEngine().removePort({.componentId = parent->getSimEngineId(),
                                   .direction = realPortDirectionFor(isInput),
                                   .signalKind = slot->getSignalKind(),
                                   .index = removedIndex});

        reindexRealSlots(sceneState, slots);

        parent->setScaleDirty();
        parent->setSchematicScaleDirty();
        parent->setSchSlotsPosDirty();
        return true;
    }

    bool SvcConnection::addSlot(
        const std::shared_ptr<Canvas::Scene> &scene,
        const std::shared_ptr<Canvas::SlotSceneComponent> &slot) {
        if (!scene) {
            BESS_ERROR("[SvcConnection] [addSlot] Invalid scene given");
            return false;
        }

        if (!slot) {
            BESS_ERROR("[SvcConnection] [addSlot] Invalid slot given");
            return false;
        }

        auto &sceneState = scene->getState();
        const auto parent =
            sceneState.getComponentByUuid<Canvas::SimulationSceneComponent>(
                slot->getParentComponent());
        if (!parent) {
            return false;
        }

        const bool isInput = slot->isInputSlot();
        const bool pairedIsInput = !isInput;
        const auto &slots =
            isInput ? parent->getInputSlots() : parent->getOutputSlots();

        const bool wasInScene = sceneState.isComponentValid(slot->getUuid());
        const bool wasParentChild =
            parent->getChildComponents().contains(slot->getUuid());
        const bool wasInParentSlots =
            std::ranges::contains(slots, slot->getUuid());

        if (!wasInParentSlots) {
            const auto insertIndex =
                restoredSlotIndex(sceneState, slots, slot->getIndex());
            if (isInput) {
                parent->insertInputSlot(slot->getUuid(), insertIndex);
            } else {
                parent->insertOutputSlot(slot->getUuid(), insertIndex);
            }
        }

        if (!wasInScene) {
            sceneState.addComponent(slot);
        }

        if (!parent->getChildComponents().contains(slot->getUuid())) {
            sceneState.attachChild(parent->getUuid(), slot->getUuid(), false);
        }

        const auto itr = std::ranges::find(slots, slot->getUuid());
        const size_t insertedIndex =
            itr == slots.end()
                ? (slots.empty() ? 0 : slots.size() - 1)
                : static_cast<size_t>(std::distance(slots.begin(), itr));
        configureSlotPort(slot, isInput, false, slot->getSignalKind());
        slot->setIndex(static_cast<int>(insertedIndex));

        reindexRealSlots(sceneState, slots);

        bool needsSimSlot = true;
        std::shared_ptr<SimEngine::Drivers::Digital::DigCompDef> digDef;
        const auto portDescriptor =
            portDescriptorFor(getSimEngine(), *parent, isInput);
        if (portDescriptor) {
            needsSimSlot =
                static_cast<size_t>(slot->getIndex()) >= portDescriptor->count;
        }
        const auto digComp =
            getSimEngine()
                .getComponent<SimEngine::Drivers::Digital::DigSimComp>(
                    parent->getSimEngineId());
        if (digComp) {
            digDef =
                digComp
                    ->getDefinition<SimEngine::Drivers::Digital::DigCompDef>();
        }

        std::shared_ptr<Canvas::SlotSceneComponent> pairedSlot;
        const std::vector<UUID> *pairedSlots = nullptr;
        bool pairedWasInScene = false;
        bool pairedWasParentChild = false;
        bool pairedWasInParentSlots = false;

        const auto findOrCreatePairedSlot =
            [&]() -> std::shared_ptr<Canvas::SlotSceneComponent> {
            if (!digDef || !digDef->getKeepIOCountEq() ||
                slot->getIndex() < 0) {
                return nullptr;
            }

            pairedSlots =
                &(isInput ? parent->getOutputSlots() : parent->getInputSlots());
            const int targetIndex = slot->getIndex();

            for (const auto &slotId : *pairedSlots) {
                const auto candidate =
                    sceneState.getComponentByUuidSP<Canvas::SlotSceneComponent>(
                        slotId);
                if (candidate && isRealSlotForSide(*candidate, pairedIsInput) &&
                    candidate->getIndex() == targetIndex) {
                    return candidate;
                }
            }

            for (const auto &childId : parent->getChildComponents()) {
                const auto candidate =
                    sceneState.getComponentByUuidSP<Canvas::SlotSceneComponent>(
                        childId);
                if (candidate && isRealSlotForSide(*candidate, pairedIsInput) &&
                    candidate->getIndex() == targetIndex) {
                    return candidate;
                }
            }

            for (const auto &[slotId, candidate] : m_slotsBin) {
                (void)slotId;
                if (candidate &&
                    candidate->getParentComponent() == parent->getUuid() &&
                    isRealSlotForSide(*candidate, pairedIsInput) &&
                    candidate->getIndex() == targetIndex) {
                    return candidate;
                }
            }

            auto created = std::make_shared<Canvas::SlotSceneComponent>();
            created->setParentComponent(parent->getUuid());
            configureSlotPort(
                created, pairedIsInput, false, slot->getSignalKind());
            created->setIndex(targetIndex);
            return created;
        };

        pairedSlot = findOrCreatePairedSlot();
        if (pairedSlot && pairedSlots) {
            pairedSlot->setParentComponent(parent->getUuid());
            configureSlotPort(
                pairedSlot, pairedIsInput, false, slot->getSignalKind());

            pairedWasInScene =
                sceneState.isComponentValid(pairedSlot->getUuid());
            pairedWasParentChild =
                parent->getChildComponents().contains(pairedSlot->getUuid());
            pairedWasInParentSlots =
                std::ranges::contains(*pairedSlots, pairedSlot->getUuid());

            if (!pairedWasInParentSlots) {
                const auto insertIndex = restoredSlotIndex(
                    sceneState, *pairedSlots, slot->getIndex());
                if (pairedIsInput) {
                    parent->insertInputSlot(pairedSlot->getUuid(), insertIndex);
                } else {
                    parent->insertOutputSlot(pairedSlot->getUuid(),
                                             insertIndex);
                }
            }

            if (!pairedWasInScene) {
                sceneState.addComponent(pairedSlot);
            }

            if (!parent->getChildComponents().contains(pairedSlot->getUuid())) {
                sceneState.attachChild(
                    parent->getUuid(), pairedSlot->getUuid(), false);
            }

            reindexRealSlots(sceneState, *pairedSlots);
        }

        if (needsSimSlot) {
            if (!getSimEngine().addPort(
                    {.componentId = parent->getSimEngineId(),
                     .direction = realPortDirectionFor(isInput),
                     .signalKind = slot->getSignalKind(),
                     .index = static_cast<int>(insertedIndex)})) {
                if (!wasParentChild) {
                    parent->removeChildComponent(slot->getUuid());
                }
                if (!wasInParentSlots) {
                    if (isInput) {
                        parent->removeInputSlot(slot->getUuid());
                    } else {
                        parent->removeOutputSlot(slot->getUuid());
                    }
                }
                if (!wasInScene &&
                    sceneState.isComponentValid(slot->getUuid())) {
                    sceneState.removeComponent(slot->getUuid(), UUID::master);
                }
                if (pairedSlot && pairedSlots) {
                    if (!pairedWasParentChild) {
                        parent->removeChildComponent(pairedSlot->getUuid());
                    }
                    if (!pairedWasInParentSlots) {
                        if (pairedIsInput) {
                            parent->removeInputSlot(pairedSlot->getUuid());
                        } else {
                            parent->removeOutputSlot(pairedSlot->getUuid());
                        }
                    }
                    if (!pairedWasInScene &&
                        sceneState.isComponentValid(pairedSlot->getUuid())) {
                        sceneState.removeComponent(pairedSlot->getUuid(),
                                                   UUID::master);
                    }
                    reindexRealSlots(sceneState, *pairedSlots);
                }
                return false;
            }
        }

        m_slotsBin.erase(slot->getUuid());
        if (pairedSlot) {
            m_slotsBin.erase(pairedSlot->getUuid());
        }

        parent->setScaleDirty();
        parent->setSchematicScaleDirty();
        parent->setSchSlotsPosDirty();
        return true;
    }

    bool SvcConnection::isResizeTriggerSlot(
        const std::shared_ptr<Canvas::SlotSceneComponent> &slot) {
        return slot && slot->isResizeSlot();
    }

    SimEngine::SimulationEngine &SvcConnection::getSimEngine() {
        BESS_ASSERT(m_sim, "Connection service has no simulation engine");
        return *m_sim;
    }

    std::pair<std::shared_ptr<Canvas::SlotSceneComponent>, bool>
    SvcConnection::tryFindSlot(const std::shared_ptr<Canvas::Scene> &scene,
                               const UUID &slotId) {
        const auto &sceneState = scene->getState();

        // try to find in scene
        const auto &slot =
            sceneState.getComponentByUuidSP<Canvas::SlotSceneComponent>(slotId);

        if (slot) {
            m_slotsBin.erase(slotId);
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

    std::optional<std::string> SvcConnection::connectInSimEngine(
        const std::shared_ptr<Canvas::Scene> &scene,
        const UUID &idA,
        const UUID &idB) {
        auto &simEngine = getSimEngine();
        const auto &sceneState = scene->getState();

        auto [slotCompA, slotCompB] = resolvePhysicalSlotPair(scene, idA, idB);

        if (!slotCompA) {
            BESS_ERROR("Failed to get physical slot component A with id {} for "
                       "connection",
                       (uint64_t)idA);
            BESS_ASSERT(false, "Failed to get slot component A for connection");
            return "Failed to get slot component A for connection";
        }

        if (!slotCompB) {
            BESS_ERROR("Failed to get physical slot component B with id {} for "
                       "connection",
                       (uint64_t)idB);
            BESS_ASSERT(false, "Failed to get slot component B for connection");
            return "Failed to get slot component B for connection";
        }

        const auto success =
            simEngine.connectPorts(slotCompA->getPortRef(sceneState),
                                   slotCompB->getPortRef(sceneState));

        if (!success) {
            BESS_WARN(
                "[ConnectionSvc] Failed to connect slots in simulation engine "
                "between component {} slot {} and component {} slot {}",
                (uint64_t)slotCompA->getParentComponent(),
                slotCompA->getIndex(),
                (uint64_t)slotCompB->getParentComponent(),
                slotCompB->getIndex());
            return "Failed to connect slots in simulation engine";
        }

        return std::nullopt;
    }

    bool SvcConnection::disconnectInSimEngine(
        const std::shared_ptr<Canvas::Scene> &scene,
        const UUID &idA,
        const UUID &idB) {
        auto &simEngine = getSimEngine();
        const auto &sceneState = scene->getState();

        auto [slotCompA, slotCompB] = resolvePhysicalSlotPair(scene, idA, idB);

        if (!slotCompA) {
            BESS_ERROR("Failed to get physical slot component A with id {} for "
                       "disconnection",
                       (uint64_t)idA);
            BESS_ASSERT(false,
                        "Failed to get slot component A for disconnection");
            return false;
        }

        if (!slotCompB) {
            BESS_ERROR("Failed to get physical slot component B with id {} for "
                       "disconnection",
                       (uint64_t)idB);
            BESS_ASSERT(false,
                        "Failed to get slot component B for disconnection");
            return false;
        }

        simEngine.deleteConnection(slotCompA->getPortRef(sceneState),
                                   slotCompB->getPortRef(sceneState));

        return true;
    }

    std::shared_ptr<Canvas::SlotSceneComponent>
    SvcConnection::getSlot(const std::shared_ptr<Canvas::Scene> &scene,
                           const UUID &compId) {
        const auto &sceneState = scene->getState();

        auto comp =
            sceneState.getComponentByUuidSP<Canvas::SlotSceneComponent>(compId);

        if (!comp) {
            BESS_ERROR("Slot component with id {} not found in scene",
                       (uint64_t)compId);
            return nullptr;
        }

        return comp;
    }

    std::pair<std::shared_ptr<Canvas::SlotSceneComponent>,
              std::shared_ptr<Canvas::SlotSceneComponent>>
    SvcConnection::resolvePhysicalSlotPair(
        const std::shared_ptr<Canvas::Scene> &scene,
        const UUID &idA,
        const UUID &idB) {
        auto &sceneState = scene->getState();

        auto getOrCastSlot =
            [&](const UUID &id) -> std::shared_ptr<Canvas::SlotSceneComponent> {
            return getSlot(scene, id);
        };

        auto proxyA = dynamic_cast<Canvas::ProxySlotComponent *>(
            sceneState.getComponentByUuid(idA));
        auto proxyB = dynamic_cast<Canvas::ProxySlotComponent *>(
            sceneState.getComponentByUuid(idB));

        auto slotA = getOrCastSlot(idA);
        auto slotB = getOrCastSlot(idB);

        if (proxyA && slotB) {
            bool isBInput = slotB->isInputSlot();
            slotA = getOrCastSlot(isBInput ? proxyA->getOutputSlotId()
                                           : proxyA->getInputSlotId());
        } else if (slotA && proxyB) {
            bool isAInput = slotA->isInputSlot();
            slotB = getOrCastSlot(isAInput ? proxyB->getOutputSlotId()
                                           : proxyB->getInputSlotId());
        } else if (proxyA && proxyB) {
            slotA = getOrCastSlot(proxyA->getOutputSlotId());
            slotB = getOrCastSlot(proxyB->getInputSlotId());
        } else {
            if (!slotA && proxyA)
                slotA = getOrCastSlot(proxyA->getOutputSlotId());
            if (!slotB && proxyB)
                slotB = getOrCastSlot(proxyB->getInputSlotId());
        }

        return {slotA, slotB};
    }

    std::optional<std::string> SvcConnection::connectProxySlots(
        const std::shared_ptr<Canvas::Scene> &scene,
        const UUID &proxyA,
        const UUID &proxyB) {
        const auto &sceneState = scene->getState();

        const auto &proxyCompA = sceneState.getComponentByUuid(proxyA);
        auto proxySlotA =
            dynamic_cast<Canvas::ProxySlotComponent *>(proxyCompA);

        const auto &proxyCompB = sceneState.getComponentByUuid(proxyB);
        auto proxySlotB =
            dynamic_cast<Canvas::ProxySlotComponent *>(proxyCompB);

        if (!proxySlotA || !proxySlotB) {
            BESS_ERROR(
                "Failed to connect proxy slots, invalid proxy components");
            BESS_ASSERT(false,
                        "Invalid proxy components for proxy-proxy connection");
            return "Invalid proxy components";
        }

        auto actualSlotAId = proxySlotA->getOutputSlotId();
        auto actualSlotBId = proxySlotB->getInputSlotId();

        // Cross-verify with resolvePhysicalSlotPair
        auto [slotA, slotB] = resolvePhysicalSlotPair(scene, proxyA, proxyB);
        if (slotA)
            actualSlotAId = slotA->getUuid();
        if (slotB)
            actualSlotBId = slotB->getUuid();

        const auto res = connectSlots(scene, actualSlotAId, actualSlotBId);

        if (res.has_value()) {
            BESS_ERROR("Failed to connect proxy slot A {} to proxy slot B {} "
                       "in sim engine, error: {}",
                       (uint64_t)proxyA,
                       (uint64_t)proxyB,
                       res.value());
            BESS_ASSERT(false,
                        "Failed to connect proxy to proxy in sim engine");
            return "Failed to connect proxy to proxy in sim engine";
        }

        return std::nullopt;
    }

    std::optional<std::string> SvcConnection::connectSlotToProxy(
        const std::shared_ptr<Canvas::Scene> &scene,
        const UUID &slotId,
        const UUID &proxyId) {
        const auto &sceneState = scene->getState();

        const auto &slotComp = getSlot(scene, slotId);
        if (!slotComp) {
            BESS_ERROR("Failed to get slot component with id {} for connecting "
                       "to proxy",
                       (uint64_t)slotId);
            BESS_ASSERT(false,
                        "Failed to get slot component for connecting to proxy");
            return "Failed to get slot component for connecting to proxy";
        }

        const auto &proxyComp = sceneState.getComponentByUuid(proxyId);
        auto proxySlot = dynamic_cast<Canvas::ProxySlotComponent *>(proxyComp);

        if (!proxySlot) {
            BESS_ERROR("Failed to get proxy slot component with id {} for "
                       "connecting to proxy",
                       (uint64_t)proxyId);
            BESS_ASSERT(
                false,
                "Failed to get proxy slot component for connecting to proxy");
            return "Failed to get proxy slot component for connecting to proxy";
        }

        UUID actualSlotId = UUID::null;

        auto [slotA, slotB] = resolvePhysicalSlotPair(scene, slotId, proxyId);
        if (slotA && slotB) {
            actualSlotId = (slotA->getUuid() == slotId) ? slotB->getUuid()
                                                        : slotA->getUuid();
        } else {
            if (slotComp->isInputSlot()) {
                actualSlotId = proxySlot->getOutputSlotId();
            } else {
                actualSlotId = proxySlot->getInputSlotId();
            }
        }

        const auto res = connectSlots(scene, slotId, actualSlotId);

        if (res.has_value()) {
            BESS_WARN("Failed to connect slot with id {} to proxy slot with id "
                      "{} in sim engine, error: {}",
                      (uint64_t)slotId,
                      (uint64_t)actualSlotId,
                      res.value());
            return "Failed to connect slot to proxy slot in sim engine";
        }

        // Update all the connections from this proxy
        for (const auto &connId : proxySlot->getConnections()) {
            const auto &connComp =
                sceneState.getComponentByUuid<Canvas::ConnectionSceneComponent>(
                    connId);

            if (!connComp) {
                continue;
            }

            const auto slotAId = connComp->getStartSlot();
            const auto slotBId = connComp->getEndSlot();

            Bess::UUID potentialId = slotAId;
            if (slotAId == actualSlotId) {
                potentialId = slotBId;
            }

            connect(scene, potentialId, slotId);
        }

        return std::nullopt;
    }

    std::optional<std::string> SvcConnection::disconnectProxySlots(
        const std::shared_ptr<Canvas::Scene> &scene,
        const UUID &proxyA,
        const UUID &proxyB) {
        auto [slotA, slotB] = resolvePhysicalSlotPair(scene, proxyA, proxyB);
        if (slotA && slotB) {
            disconnectInSimEngine(scene, slotA->getUuid(), slotB->getUuid());
        }
        return std::nullopt;
    }

    std::optional<std::string> SvcConnection::disconnectSlotFromProxy(
        const std::shared_ptr<Canvas::Scene> &scene,
        const UUID &slotId,
        const UUID &proxyId) {
        auto [slotA, slotB] = resolvePhysicalSlotPair(scene, slotId, proxyId);
        if (slotA && slotB) {
            disconnectInSimEngine(scene, slotA->getUuid(), slotB->getUuid());
        }

        // Disconnect all cascaded mappings that piggy-backed through this proxy
        // joint
        const auto &sceneState = scene->getState();
        const auto &proxyComp = sceneState.getComponentByUuid(proxyId);
        auto proxySlot = dynamic_cast<Canvas::ProxySlotComponent *>(proxyComp);
        if (proxySlot) {
            for (const auto &connId : proxySlot->getConnections()) {
                const auto &connComp =
                    sceneState
                        .getComponentByUuid<Canvas::ConnectionSceneComponent>(
                            connId);
                if (!connComp)
                    continue;

                const auto loopSlotAId = connComp->getStartSlot();
                const auto loopSlotBId = connComp->getEndSlot();

                Bess::UUID potentialId = loopSlotAId;
                if (loopSlotAId == proxyId)
                    potentialId = loopSlotBId;

                // Unlink underlying physical structures that the proxy
                // previously connected
                auto [resolvedA, resolvedB] =
                    resolvePhysicalSlotPair(scene, potentialId, slotId);
                if (resolvedA && resolvedB) {
                    disconnectInSimEngine(
                        scene, resolvedA->getUuid(), resolvedB->getUuid());
                }
            }
        }

        return std::nullopt;
    }

    std::optional<std::string>
    SvcConnection::connectSlots(const std::shared_ptr<Canvas::Scene> &scene,
                                const UUID &slotAId,
                                const UUID &slotBId) {
        return connectInSimEngine(scene, slotAId, slotBId);
    }

    std::optional<std::string>
    SvcConnection::connect(const std::shared_ptr<Canvas::Scene> &scene,
                           const UUID &idA,
                           const UUID &idB) {
        const auto &sceneState = scene->getState();

        // figure out types of two and call the correct connect function
        const auto &compA = sceneState.getComponentByUuid(idA);
        const auto &compB = sceneState.getComponentByUuid(idB);

        if (!compA) {
            BESS_ERROR(
                "Component A with id {} not found in scene for connecting",
                (uint64_t)idA);
            BESS_ASSERT(false, "Component A not found in scene for connecting");
            return "Component A not found in scene for connecting";
        }

        if (!compB) {
            BESS_ERROR(
                "Component B with id {} not found in scene for connecting",
                (uint64_t)idB);
            BESS_ASSERT(false, "Component B not found in scene for connecting");
            return "Component B not found in scene for connecting";
        }

        const auto &typeA = compA->getType();
        const auto &typeB = compB->getType();

        // Both are slots
        if (typeA == Canvas::SceneComponentType::slot &&
            typeB == Canvas::SceneComponentType::slot) {
            return connectSlots(scene, idA, idB);
        }

        // Type A is slot then assume slot B is proxy slot
        if (typeA == Canvas::SceneComponentType::slot) {
            const auto &proxySlot =
                dynamic_cast<Canvas::ProxySlotComponent *>(compB);
            if (!proxySlot) {
                BESS_ERROR(
                    "Component B with id {} is not a proxy slot component",
                    (uint64_t)idB);
                BESS_ASSERT(false, "Component B is not a proxy slot component");
                return "Component B is not a proxy slot component";
            }

            return connectSlotToProxy(scene, idA, idB);
        }

        // Type B is slot then assume slot A is proxy slot
        if (typeB == Canvas::SceneComponentType::slot) {
            const auto &proxySlot =
                dynamic_cast<Canvas::ProxySlotComponent *>(compA);
            if (!proxySlot) {
                BESS_ERROR(
                    "Component A with id {} is not a proxy slot component",
                    (uint64_t)idA);
                BESS_ASSERT(false, "Component A is not a proxy slot component");
                return "Component A is not a proxy slot component";
            }

            return connectSlotToProxy(scene, idB, idA);
        }

        // Both are proxy slots
        const auto &proxySlotA =
            dynamic_cast<Canvas::ProxySlotComponent *>(compA);
        const auto &proxySlotB =
            dynamic_cast<Canvas::ProxySlotComponent *>(compB);

        if (!proxySlotA) {
            BESS_ERROR("Component A with id {} is not a proxy slot component",
                       (uint64_t)idA);
            BESS_ASSERT(false, "Component A is not a proxy slot component");
            return "Component A is not a proxy slot component";
        }

        if (!proxySlotB) {
            BESS_ERROR("Component B with id {} is not a proxy slot component",
                       (uint64_t)idB);
            BESS_ASSERT(false, "Component B is not a proxy slot component");
            return "Component B is not a proxy slot component";
        }

        return connectProxySlots(scene, idA, idB);
    }

    std::optional<std::string>
    SvcConnection::disconnect(const std::shared_ptr<Canvas::Scene> &scene,
                              const UUID &idA,
                              const UUID &idB) {
        const auto &sceneState = scene->getState();

        const auto &compA = sceneState.getComponentByUuid(idA);
        const auto &compB = sceneState.getComponentByUuid(idB);

        if (!compA || !compB) {
            BESS_ERROR("Components not found in scene for disconnecting "
                       "endpoints: {} and {}",
                       (uint64_t)idA,
                       (uint64_t)idB);
            return "Components not found";
        }

        const auto &typeA = compA->getType();
        const auto &typeB = compB->getType();

        if (typeA == Canvas::SceneComponentType::slot &&
            typeB == Canvas::SceneComponentType::slot) {
            disconnectInSimEngine(scene, idA, idB);
            return std::nullopt;
        }

        if (typeA == Canvas::SceneComponentType::slot)
            return disconnectSlotFromProxy(scene, idA, idB);
        if (typeB == Canvas::SceneComponentType::slot)
            return disconnectSlotFromProxy(scene, idB, idA);

        return disconnectProxySlots(scene, idA, idB);
    }

    std::optional<std::string>
    SvcConnection::regConnToComp(const std::shared_ptr<Canvas::Scene> &scene,
                                 const UUID &compId,
                                 const UUID &connId) {

        auto &sceneState = scene->getState();

        const auto &comp = sceneState.getComponentByUuid(compId);

        if (!comp) {
            BESS_ERROR("Component with id {} not found in scene for "
                       "registering connection",
                       (uint64_t)compId);
            BESS_ASSERT(
                false,
                "Component not found in scene for registering connection");
            return "Component not found in scene for registering connection";
        }

        if (comp->getType() == Canvas::SceneComponentType::slot) {
            const auto &slotComp = comp->cast<Canvas::SlotSceneComponent>();
            slotComp->addConnection(connId);
        } else {
            const auto &proxy =
                dynamic_cast<Canvas::ProxySlotComponent *>(comp);
            if (!proxy) {
                BESS_ERROR("Component with id {} is not a proxy slot component "
                           "for registering connection",
                           (uint64_t)compId);
                BESS_ASSERT(false,
                            "Component is not a proxy slot component "
                            "for registering connection");
                return "Component is not a proxy slot component for "
                       "registering connection";
            }
            proxy->addConnection(connId);
        }

        return std::nullopt;
    }

    std::shared_ptr<Canvas::SlotSceneComponent>
    SvcConnection::createSlotFromResizeTrigger(
        const std::shared_ptr<Canvas::Scene> &scene,
        const std::shared_ptr<Canvas::SlotSceneComponent> &resizeSlot) {
        if (!resizeSlot || !resizeSlot->isResizeSlot()) {
            return nullptr;
        }

        auto &sceneState = scene->getState();
        const auto parent =
            sceneState.getComponentByUuid<Canvas::SimulationSceneComponent>(
                resizeSlot->getParentComponent());
        if (!parent) {
            return nullptr;
        }

        auto newSlot = std::make_shared<Canvas::SlotSceneComponent>();
        newSlot->setParentComponent(parent->getUuid());
        newSlot->setPortDirection(resizeSlot->getPortDirection());
        newSlot->setSignalKind(resizeSlot->getSignalKind());
        newSlot->setResizeTrigger(false);

        if (!addSlot(scene, newSlot)) {
            return nullptr;
        }

        return newSlot;
    }

    std::pair<bool, std::string>
    SvcConnection::canConnect(const UUID &idA,
                              const UUID &idB,
                              const std::shared_ptr<Canvas::Scene> &scene) {
        if (!scene) {
            return {false, "Missing scene for connection check"};
        }

        auto &simEngine = getSimEngine();
        const auto &sceneState = scene->getState();

        auto [slotCompA, slotCompB] = resolvePhysicalSlotPair(scene, idA, idB);

        if (!slotCompA || !slotCompB) {
            return {false,
                    "Invalid slot components for connection check "
                    "(Proxy links might be dead)"};
        }

        const auto portForCheck =
            [&](const std::shared_ptr<Canvas::SlotSceneComponent> &slot) {
                auto port = slot->getPortRef(sceneState);
                if (slot->isResizeSlot()) {
                    const auto parent = sceneState.getComponentByUuid<
                        Canvas::SimulationSceneComponent>(
                        slot->getParentComponent());
                    if (parent) {
                        const auto &slots = slot->isInputSlot()
                                                ? parent->getInputSlots()
                                                : parent->getOutputSlots();
                        port.index =
                            static_cast<int>(realSlotEnd(sceneState, slots));
                    }
                }
                return port;
            };

        const auto portA = portForCheck(slotCompA);
        const auto portB = portForCheck(slotCompB);

        if (!portA.isValid() || !portB.isValid()) {
            return {
                false,
                "Missing parent simulation components for connection check"};
        }

        if (portA.direction == portB.direction) {
            return {false,
                    "Cannot connect pins of the same type i.e. input -> "
                    "input or output -> output"};
        }

        if (portA.signalKind != portB.signalKind) {
            return {false, "Cannot connect ports with different signal kinds"};
        }

        const bool isResizeA = slotCompA->isResizeSlot();
        const bool isResizeB = slotCompB->isResizeSlot();

        if (isResizeA || isResizeB) {
            // A resize slot inherently expands the bounds, representing a new
            // valid pin. Since we've validated the type constraints above, we
            // can safely allow the topological intent.
            return {true, ""};
        }

        return simEngine.canConnectPorts(portA, portB);
    }

} // namespace Bess::Svc
