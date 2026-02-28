#pragma once

/// Service is responsible to create, remove and maintain the connections in the scene.
/// To update connections any where use this service only.

#include "pages/main_page/scene_components/connection_scene_component.h"
#include <memory>

namespace Bess {
    class UUID;
}

namespace Bess::Canvas {
    class Scene;
    class ConnectionSceneComponent;
    class SlotSceneComponent;
} // namespace Bess::Canvas

namespace Bess::SimEngine {
    class SimulationEngine;
}

namespace Bess::Svc {
    class SvcConnection {
      public:
        static SvcConnection &instance();

        void init();
        void destroy();

        // Takes a connection component and tries to add it to the correct place
        // @returns: true on sucess and false otherwise
        bool addConnection(const std::shared_ptr<Canvas::ConnectionSceneComponent> &conn);

        // Takes a connection component and tries to remove it safely
        // @returns: ids of all the components which were removed (includes slots if any were removed),
        // empty on fail
        std::vector<UUID> removeConnection(const std::shared_ptr<Canvas::ConnectionSceneComponent> &conn);

        // Takes a connection id and returns the ids of all the components which are life dependants on it,
        // empty if no dependants or connection not found.
        std::vector<UUID> getDependants(const UUID &connection);

      private:
        // Takes a slot/proxy id and a connection id, and add it to its connection list,
        // @returns: error message on failure, empty on success
        std::optional<std::string> regConnToComp(const UUID &compId, const UUID &connId);

        // Takes two slot/proxy ids and tries to connect them together in the scene and sim engine,
        // @returns: error message on failure, empty on success
        std::optional<std::string> connect(const UUID &idA, const UUID &idB);

        // Takes two proxy slot ids (scene component ids) and connects them together
        // by creating a connection between relevant slots
        // In Debug: Will assert if ids are not of valid Proxy Slot Components
        // @returns: error message on failure, empty on success
        std::optional<std::string> connectProxySlots(const UUID &proxyA, const UUID &proxyB);

        // Takes a slot id and a proxy slot id (scene slot component) and connects them together
        // by creating a connection between them,
        // it picks the correct and relvant slot from the proxy slot component to connect to the given slot.
        // @returns: error message on failure
        std::optional<std::string> connectSlotToProxy(const UUID &slotId, const UUID &proxyId);

        // Takes two slot ids (scene slots) and connects them together by creating a connection between them
        // @returns: error message on failure, empty on success
        std::optional<std::string> connectSlots(const UUID &slotAId, const UUID &slotBId);

        // Takes two slots/proxy ids and connects them in sim engine
        // @returns: error message on failure, empty on success
        std::optional<std::string> connectInSimEngine(const UUID &idA, const UUID &idB);

        // Takes two slot/proxy ids and disconnects them in sim engine,
        // @returns: true on sucess and false otherwise
        bool disconnectInSimEngine(const UUID &idA, const UUID &idB);

        // Checks if given slot is a resize slot,
        // which means we will add new slot inplace of it,
        // and move it down.
        bool isResizeTriggerSlot(const std::shared_ptr<Canvas::SlotSceneComponent> &slot);

        // Checks if slot is safe to remove, true if following are met.
        // If its part of resizeable slot group,
        // If its the last slot and not resize slot,
        // and has less than or equal to given threshold connections.
        bool isSlotRemovable(const std::shared_ptr<Canvas::SlotSceneComponent> &slot,
                             size_t connectionThreshold = 0);

        // Removes the slot from the sim componenet,
        // and decrements count in sim engine digital component.
        // @returns: true on sucess and false otherwise
        bool removeSlot(const std::shared_ptr<Canvas::SlotSceneComponent> &slot);

        // Adds slot to the parent and increments the count of sim engine digital component.
        // @returns: true on sucess and false otherwise
        bool addSlot(const std::shared_ptr<Canvas::SlotSceneComponent> &slot);

      private:
        // Tries to find the slot in the scene.
        // If not found in scene then in the internal bin.
        // If found in bin then it removes it from the bin and returns it,
        // otherwise returns nullptr.
        // @returns: pair,
        // first (comp): nullptr if not found, otherwise the slot.
        // second (bool): true if found in scene, false if found in bin or not found at all.
        std::pair<std::shared_ptr<Canvas::SlotSceneComponent>, bool> tryFindSlot(const UUID &slotId);

        // Checks if comp is proxy or slot and returns the slot component correctly.
        std::shared_ptr<Canvas::SlotSceneComponent> getSlot(const UUID &compId);

      private:
        std::shared_ptr<Canvas::Scene> getScene();
        SimEngine::SimulationEngine &getSimEngine();

      private:
        // Bin for the slots that are not in the scene,
        // contains the slots which were removed
        // when they were redundant after connection removal.
        // Key is the slot id and value is the slot component.
        std::unordered_map<UUID, std::shared_ptr<Canvas::SlotSceneComponent>> m_slotsBin;

      private:
        SvcConnection() = default;
        ~SvcConnection() = default;
        SvcConnection(const SvcConnection &) = delete;
        SvcConnection &operator=(const SvcConnection &) = delete;
    };
} // namespace Bess::Svc
