#pragma once

#include "component_types.h"
#include "entt/entity/fwd.hpp"
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

    enum PinType {
        input,
        output
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

        entt::entity addComponent(ComponentType type);
        ComponentState getComponentState(entt::entity entity);
        bool connectComponent(entt::entity src, int srcPin, PinType srcPinType, entt::entity dst, int dstPin, PinType dstPinType);
        void deleteComponent(entt::entity component);
        void deleteConnection(entt::entity componentA, PinType pinAType, int idx, entt::entity componentB, PinType pinBType, int idxB);

        void setDigitalInput(entt::entity entity, bool value);
        bool getDigitalPinState(entt::entity entity, PinType type, int idx);
        ComponentType getComponentType(entt::entity entity);

      private:
        entt::registry registry;
        std::thread simThread;
        std::mutex queueMutex;
        std::condition_variable cv;
        bool stopFlag;
        std::priority_queue<SimulationEvent> eventQueue;

        // Main simulation loop.
        void run();

        std::vector<bool> getInputPinsState(entt::entity gateEntt);
    };

} // namespace Bess::SimEngine
