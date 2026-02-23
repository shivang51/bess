#pragma once
#include "bess_api.h"
#include "common/bess_uuid.h"
#include "digital_component.h"
#include "net/net.h"
#include "sim_engine_state.h"
#include "types.h"
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <set>
#include <thread>

namespace Bess::SimEngine {
    class ComponentDefinition;

    class BESS_API SimulationEngine {
      public:
        static SimulationEngine &instance();

        SimulationEngine();
        ~SimulationEngine();

        void destroy();

        const UUID &addComponent(const std::shared_ptr<ComponentDefinition> &definition);

        bool connectComponent(const UUID &src, int srcSlotIdx, SlotType srcType,
                              const UUID &dst, int dstSlotIdx, SlotType dstType, bool overrideConn = false);

        // returns {canConnect, errorMessage}
        std::pair<bool, std::string> canConnectComponents(const UUID &src, int srcSlotIdx, SlotType srcType,
                                                          const UUID &dst, int dstSlotIdx, SlotType dstType) const;

        void deleteComponent(const UUID &uuid);

        void deleteConnection(const UUID &compA, SlotType pinAType, int idxA,
                              const UUID &compB, SlotType pinBType, int idxB);

        SlotState getDigitalSlotState(const UUID &uuid, SlotType type, int idx);

        ConnectionBundle getConnections(const UUID &uuid);

        void setInputSlotState(const UUID &uuid, int pinIdx, LogicState state);
        void setOutputSlotState(const UUID &uuid, int pinIdx, LogicState state);
        void invertInputSlotState(const UUID &uuid, int pinIdx);

        SimTime getSimulationTime() const;
        std::chrono::milliseconds getSimulationTimeMS();
        std::chrono::seconds getSimulationTimeS();

        SimulationState toggleSimState();
        SimulationState getSimulationState() const;
        void setSimulationState(SimulationState state);

        // only steps if sim state is paused
        void stepSimulation();

        const ComponentState &getComponentState(const UUID &uuid);
        const std::shared_ptr<ComponentDefinition> &getComponentDefinition(const UUID &uuid) const;
        std::shared_ptr<DigitalComponent> getDigitalComponent(const UUID &uuid);

        void clear();

        bool updateInputCount(const UUID &uuid, int n);

        std::vector<std::pair<float, bool>> getStateMonitorData(UUID uuid);

        bool updateNets(const std::vector<UUID> &startCompIds);

        friend class SimEngineSerializer;

        bool isNetUpdated() const;

        const std::unordered_map<UUID, Net> &getNetsMap();

        TruthTable getTruthTableOfNet(const UUID &netUuid);

        bool isSimStable();

        const SimEngineState &getSimEngineState() const;
        SimEngineState &getSimEngineState();

      private:
        bool isSimStableLocked() const;

        std::vector<UUID> getConnGraph(UUID start);

        void scheduleEvent(UUID id, UUID schedulerId, SimDelayNanoSeconds simTime);
        void clearEventsForEntity(const UUID &id);
        std::vector<SlotState> getInputSlotsState(UUID compId) const;
        bool simulateComponent(const UUID &compId, const std::vector<SlotState> &inputs);
        void run();

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

        SimEngineState m_simEngineState;

        std::unordered_map<UUID, Net> m_nets;

        bool m_destroyed{false};

        bool m_isNetUpdated{false};
        bool m_isSimulating{false};
    };
} // namespace Bess::SimEngine
