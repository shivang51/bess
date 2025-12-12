#pragma once
#include "bess_api.h"
#include "bess_uuid.h"
#include "commands/commands_manager.h"
#include "entt/entity/fwd.hpp"
#include "expression_evalutator/expr_evaluator.h"
#include "net/net.h"
#include "types.h"
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <entt/entt.hpp>
#include <mutex>
#include <thread>

namespace Bess::SimEngine {
    class ComponentDefinition;

    class BESS_API SimulationEngine {
      public:
        static SimulationEngine &instance();

        SimulationEngine();
        ~SimulationEngine();

        void destroy();

        const UUID &addComponent(const ComponentDefinition &definition, int inputCount = -1, int outputCount = -1);

        bool connectComponent(const UUID &src, int srcPin, PinType srcType,
                              const UUID &dst, int dstPin, PinType dstType, bool overrideConn = false);

        void deleteComponent(const UUID &uuid);

        void deleteConnection(const UUID &compA, PinType pinAType, int idxA,
                              const UUID &compB, PinType pinBType, int idxB);

        PinState getDigitalPinState(const UUID &uuid, PinType type, int idx);

        ConnectionBundle getConnections(const UUID &uuid);

        void setInputPinState(const UUID &uuid, int pinIdx, LogicState state);
        void setOutputPinState(const UUID &uuid, int pinIdx, LogicState state);
        void invertInputPinState(const UUID &uuid, int pinIdx);

        bool updateClock(const UUID &uuid, bool enable, float frequency, FrequencyUnit unit);

        std::chrono::milliseconds getSimulationTimeMS();
        std::chrono::seconds getSimulationTimeS();

        SimulationState toggleSimState();
        SimulationState getSimulationState();
        void setSimulationState(SimulationState state);

        // only steps if sim state is paused
        void stepSimulation();

        const ComponentState &getComponentState(const UUID &uuid);
        const ComponentDefinition &getComponentDefinition(const UUID &uuid);

        void clear();

        Commands::CommandsManager &getCmdManager();

        template <typename EnttComponentType>
        EnttComponentType &getEnttComp(const UUID &uuid) {
            auto ent = getEntityWithUuid(uuid);
            return m_registry.get<EnttComponentType>(ent);
        }
        bool updateInputCount(const UUID &uuid, int n);

        std::vector<std::pair<float, bool>> getStateMonitorData(UUID uuid);

        bool updateNets(const std::vector<entt::entity> &startEntities);

        friend class SimEngineSerializer;

        bool isNetUpdated() const;

        const std::unordered_map<UUID, Net> &getNetsMap();

        const std::vector<std::vector<LogicState>> getTruthTableOfNet(const UUID &netUuid);

        bool isSimStable();

      private:
        std::vector<entt::entity> getConnGraph(entt::entity start);

        void scheduleEvent(entt::entity e, entt::entity schedulerEntity, SimDelayNanoSeconds simTime);
        void clearEventsForEntity(entt::entity e);
        std::vector<PinState> getInputPinsState(entt::entity e) const;
        bool simulateComponent(entt::entity e, const std::vector<PinState> &inputs);
        void run();

        entt::entity getEntityWithUuid(const UUID &uuid) const;

        std::thread m_simThread;

        std::chrono::steady_clock m_realWorldClock;

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

        Commands::CommandsManager m_cmdManager;

        std::unordered_map<UUID, Net> m_nets;

        bool m_destroyed{false};

        bool m_isNetUpdated{false};
    };
} // namespace Bess::SimEngine
