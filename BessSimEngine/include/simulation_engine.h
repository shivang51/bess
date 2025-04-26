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
#include <queue>
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
        void setDigitalInput(const UUID &uuid, bool value);
        bool updateClock(const UUID &uuid, bool enable, float frequency, FrequencyUnit unit);

        ComponentState getComponentState(const UUID &uuid);
        ComponentType getComponentType(const UUID &uuid);

        friend class SimEngineSerializer;

      private:
        void scheduleEvent(entt::entity e, entt::entity schedulerEntity, SimDelayMilliSeconds t);
        void clearEventsForEntity(entt::entity e);
        std::vector<bool> getInputPinsState(entt::entity e) const;
        bool simulateComponent(entt::entity e, const std::vector<bool> &inputs);
        void run();

        entt::entity getEntityWithUuid(const UUID &uuid) const;

        std::thread simThread;
        std::atomic<bool> stopFlag{false};
        mutable std::mutex queueMutex;
        std::condition_variable cv;

        std::set<SimulationEvent> eventSet;
        uint64_t nextEventId{0};
        std::chrono::milliseconds currentSimTime;

        mutable std::mutex registryMutex;
        entt::registry m_registry;
        std::unordered_map<UUID, entt::entity> uuidMap;
    };
} // namespace Bess::SimEngine
