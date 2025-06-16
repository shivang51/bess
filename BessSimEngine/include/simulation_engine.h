#pragma once
#include "bess_api.h"
#include "bess_uuid.h"
#include "component_types.h"
#include "entt/entity/fwd.hpp"
#include "types.h"
#include <chrono>
#include <condition_variable>
#include <entt/entt.hpp>
#include <mutex>
#include <thread>

namespace Bess::SimEngine {

    class BESS_API SimulationEngine {
      public:
        static SimulationEngine &instance();

        SimulationEngine();
        ~SimulationEngine();

        const UUID &addComponent(ComponentType type, int inputCount = -1, int outputCount = -1);

        bool connectComponent(const UUID &src, int srcPin, PinType srcType,
                              const UUID &dst, int dstPin, PinType dstType);

        void deleteComponent(const UUID &uuid);

        void deleteConnection(const UUID &compA, PinType pinAType, int idxA,
                              const UUID &compB, PinType pinBType, int idxB);

        bool getDigitalPinState(const UUID &uuid, PinType type, int idx);

        ConnectionBundle getConnections(const UUID &uuid);

        void setDigitalInput(const UUID &uuid, bool value);

        bool updateClock(const UUID &uuid, bool enable, float frequency, FrequencyUnit unit);

        std::chrono::milliseconds getSimulationTimeMS();

        SimulationState toggleSimState();
        SimulationState getSimulationState();
        void setSimulationState(SimulationState state);

        // only steps if sim state is paused
        void stepSimulation();

        ComponentState getComponentState(const UUID &uuid);
        ComponentType getComponentType(const UUID &uuid);

        void clear();

        friend class SimEngineSerializer;

      private:
        void scheduleEvent(entt::entity e, entt::entity schedulerEntity, SimDelayMilliSeconds t);
        void clearEventsForEntity(entt::entity e);
        std::vector<bool> getInputPinsState(entt::entity e) const;
        std::pair<std::vector<bool>, std::vector<bool>> getIOPinsConnectedState(entt::entity e) const;
        bool simulateComponent(entt::entity e, const std::vector<bool> &inputs);
        void run();

        entt::entity getEntityWithUuid(const UUID &uuid) const;

        std::thread m_simThread;

        mutable std::mutex m_queueMutex;
        mutable std::mutex m_stateMutex;
        mutable std::mutex m_registryMutex;

        std::atomic<bool> m_stopFlag{false};
        std::atomic<bool> m_stepFlag{false};
        std::atomic<SimulationState> m_simState;
        std::condition_variable m_queueCV;
        std::condition_variable m_stateCV;

        std::set<SimulationEvent> m_eventSet;
        uint64_t m_nextEventId{0};
        SimTime m_currentSimTime;

        entt::registry m_registry;
        std::unordered_map<UUID, entt::entity> m_uuidMap;
    };
} // namespace Bess::SimEngine
