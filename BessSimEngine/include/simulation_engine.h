#pragma once

#include "bess_uuid.h"
#include "component_types.h"
#include "entt/entity/fwd.hpp"
#include "types.h"
#include <chrono>
#include <condition_variable>
#include <entt/entt.hpp>
#include <mutex>
#include <queue>
#include <thread>

namespace Bess::SimEngine {
    // Represents a scheduled simulation event.
    struct SimulationEvent {
        std::chrono::steady_clock::time_point time;
        entt::entity entity;
        // For the priority queue: earlier times have higher priority.
        bool operator<(const SimulationEvent &other) const;
    };

    struct ComponentState {
        std::vector<bool> inputStates;
        std::vector<bool> outputStates;
    };

    class SimulationEngine {
      public:
        // Constructor: pass a reference to an entt registry.
        SimulationEngine();
        ~SimulationEngine();

        static SimulationEngine &instance();

        // Stop the simulation thread.
        void stop();

        // Schedule an event for a given entity at the specified time.
        void scheduleEvent(entt::entity entity, std::chrono::steady_clock::time_point time);
        bool simulateComponent(entt::entity e);

        const UUID &addComponent(ComponentType type, int inputCount = -1, int outputCount = -1);
        ComponentState getComponentState(const UUID &entity);
        bool connectComponent(const UUID &src, int srcPin, PinType srcPinType, const UUID &dst, int dstPin, PinType dstPinType);
        void deleteComponent(const UUID &component);
        void deleteConnection(const UUID &componentA, PinType pinAType, int idx, const UUID &componentB, PinType pinBType, int idxB);

        void setDigitalInput(const UUID &entity, bool value);
        bool getDigitalPinState(const UUID &entity, PinType type, int idx);
        ComponentType getComponentType(const UUID &entity);

        bool updateClock(const UUID &uuid, bool shouldClock, float frequency, FrequencyUnit unit);

        friend class SimEngineSerializer;

      private:
        void clearEventsForEntity(entt::entity entity);
        entt::registry m_registry;
        std::thread simThread;
        std::mutex queueMutex;
        std::condition_variable cv;
        bool stopFlag;
        std::priority_queue<SimulationEvent> eventQueue;
        entt::entity getEntityWithUuid(const UUID &uuid);
        const UUID &getUuidOfEntity(entt::entity ent);

        // Main simulation loop.
        void run();

        std::vector<bool> getInputPinsState(entt::entity gateEntt);
    };

} // namespace Bess::SimEngine
