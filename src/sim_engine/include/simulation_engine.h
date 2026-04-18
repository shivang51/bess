#pragma once

#include "analog_simulation.h"
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
        struct SlotEndpoint {
            UUID componentId = UUID::null;
            int slotIdx = -1;
            SlotType slotType = SlotType::digitalInput;
        };

        static SimulationEngine &instance();

        SimulationEngine();
        ~SimulationEngine();

        void destroy();

        const UUID &addComponent(const std::shared_ptr<ComponentDefinition> &definition,
                                 bool cloneDef = true);

        bool connectComponent(const UUID &src, int srcSlotIdx, SlotType srcType,
                              const UUID &dst, int dstSlotIdx, SlotType dstType, bool overrideConn = false);

        // returns {canConnect, errorMessage}
        std::pair<bool, std::string> canConnectComponents(const UUID &src, int srcSlotIdx, SlotType srcType,
                                                          const UUID &dst, int dstSlotIdx, SlotType dstType) const;

        std::pair<bool, std::string> canConnectSlots(const SlotEndpoint &a,
                       const SlotEndpoint &b) const;
        bool connectSlots(const SlotEndpoint &a, const SlotEndpoint &b);
        bool disconnectSlots(const SlotEndpoint &a, const SlotEndpoint &b);

        void deleteComponent(const UUID &uuid);

        void deleteConnection(const UUID &compA, SlotType pinAType, int idxA,
                              const UUID &compB, SlotType pinBType, int idxB);

        SlotState getDigitalSlotState(const UUID &uuid, SlotType type, int idx) const;
        SlotState getSlotState(const UUID &uuid, SlotType type, int idx) const;
        bool isSlotConnected(const UUID &uuid, SlotType type, int idx) const;
        bool isComponentValid(const UUID &uuid) const;

        ConnectionBundle getConnections(const UUID &uuid);
        std::vector<SlotState> getInputSlotsState(UUID compId) const;

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
        std::shared_ptr<DigitalComponent> getDigitalComponent(const UUID &uuid) const;

        void clear();

        bool updateInputCount(const UUID &uuid, int n);

        std::vector<std::pair<float, bool>> getStateMonitorData(UUID uuid);

        bool updateNets(const std::vector<UUID> &startCompIds);

        friend class SimEngineSerializer;

        bool isNetUpdated() const;

        // if update is false, the sync flag will not be reset
        const std::unordered_map<UUID, Net> &getNetsMap(bool update = true);

        TruthTable getTruthTableOfNet(const UUID &netUuid);

        bool isSimStable();

        const SimEngineState &getSimEngineState() const;
        SimEngineState &getSimEngineState();

        AnalogCircuit &getAnalogCircuit();
        const AnalogCircuit &getAnalogCircuit() const;
        const UUID &addAnalogComponent(std::shared_ptr<AnalogComponent> component,
                     const std::shared_ptr<ComponentDefinition> &definition = nullptr);
        const UUID &addAnalogResistor(double resistanceOhms, const std::string &name = {});
        const UUID &addAnalogResistor(AnalogNodeId a, AnalogNodeId b, double resistanceOhms,
                                      const std::string &name = {});
        bool connectAnalogTerminal(const UUID &componentId, size_t terminalIdx, AnalogNodeId node);
        bool disconnectAnalogTerminal(const UUID &componentId, size_t terminalIdx);
        bool removeAnalogComponent(const UUID &componentId);
        AnalogComponentState getAnalogComponentState(const UUID &componentId) const;
        AnalogSolution solveAnalogCircuit(const AnalogSolveOptions &options = {});
        void clearAnalogCircuit();

      private:
        struct AnalogTerminalRef {
            UUID componentId = UUID::null;
            size_t terminalIdx = 0;
        };

        struct AnalogTerminalConnection {
            AnalogTerminalRef a;
            AnalogTerminalRef b;
        };

        bool isSimStableLocked() const;

        std::vector<UUID> getConnGraph(UUID start);

        std::pair<bool, std::string> canConnectComponentsLocked(const UUID &src, int srcSlotIdx, SlotType srcType,
                                                                const UUID &dst, int dstSlotIdx, SlotType dstType) const;
        std::pair<bool, std::string> canConnectSlotsLocked(const SlotEndpoint &a,
                       const SlotEndpoint &b) const;
        bool validateAnalogTerminalLocked(const UUID &componentId,
                  int slotIdx,
                  std::string &error) const;
        static AnalogTerminalConnection normalizeAnalogConnection(const AnalogTerminalRef &a,
                        const AnalogTerminalRef &b);
        bool hasAnalogConnectionLocked(const AnalogTerminalConnection &connection) const;
        bool removeAnalogConnectionLocked(const AnalogTerminalConnection &connection);
        void removeAnalogConnectionsForComponentLocked(const UUID &componentId);
        void rebuildAnalogConnectionsLocked();
        void scheduleEvent(UUID id, UUID schedulerId, SimDelayNanoSeconds simTime);
        void clearEventsForEntity(const UUID &id);
        bool simulateComponent(const UUID &compId, const std::vector<SlotState> &inputs);
        void scheduleDependantsOf(const UUID &compId);
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
        AnalogCircuit m_analogCircuit;
        AnalogSolution m_lastAnalogSolution;
        std::vector<AnalogTerminalConnection> m_analogConnections;
        std::unordered_map<UUID, std::shared_ptr<ComponentDefinition>> m_analogComponentDefinitions;

        std::unordered_map<UUID, Net> m_nets;

        bool m_destroyed{false};

        bool m_isNetUpdated{false};
        bool m_isSimulating{false};
    };
} // namespace Bess::SimEngine
