#pragma once

#include "bess_api.h"
#include "common/bess_uuid.h"
#include "digital_component.h"
#include "drivers/digital_sim_driver.h"
#include "drivers/sim_driver.h"
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

        void destroy();

        const UUID &addComponent(const std::shared_ptr<Drivers::CompDef> &definition,
                                 bool cloneDef = true);

        template <typename T>
        std::shared_ptr<T> getComponent(const UUID &uuid) const {
            for (const auto &driver : m_simDrivers) {
                auto comp = driver->template getComponent<T>(uuid);
                if (comp) {
                    return comp;
                }
            }

            return nullptr;
        }

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
        void clearPendingDriverEvents();

        // only steps if sim state is paused
        void stepSimulation();

        const ComponentState &getComponentState(const UUID &uuid);
        const std::shared_ptr<Drivers::CompDef> &getComponentDefinition(
            const UUID &uuid) const;
        std::shared_ptr<Drivers::Digital::DigSimComp> getDigitalComponent(const UUID &uuid) const;

        void clear();

        bool updateInputCount(const UUID &uuid, int n);

        bool addSlot(const UUID &compId, SlotType type, int index);
        bool removeSlot(const UUID &compId, SlotType type, int index);

        bool updateNets(const std::vector<UUID> &startCompIds);

        friend class SimEngineSerializer;

        bool isNetUpdated() const;
        void setNetUpdated(bool updated);

        // if update is false, the sync flag will not be reset
        const std::unordered_map<UUID, Net> &getNetsMap(bool update = true);
        std::unordered_map<UUID, Net> &getNetsMapMutable();

        void triggerPropagation(const UUID &sourceId);
        void markPendingSignalSource(const UUID &sourceId);

        TruthTable getTruthTableOfNet(const UUID &netUuid);

        bool isSimStable();

        const SimEngineState &getSimEngineState() const;
        SimEngineState &getSimEngineState();

      private:
        void loadDrivers();
        void unloadDrivers();

        void initDrivers();
        void destroyDrivers();

        void runDrivers();
        void stopDrivers();

      private:
        SimulationEngine();
        ~SimulationEngine();

      private:
        bool isSimStableLocked() const;

        void propagateFromComponent(const UUID &sourceId);
        void processPendingPropagation();

        std::vector<UUID> getConnGraph(UUID start);

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
        std::atomic<SimulationState> m_simState{SimulationState::running};
        std::condition_variable m_queueCV;
        std::condition_variable m_stateCV;

        std::set<SimulationEvent> m_eventSet;
        std::set<UUID> m_pendingSignalSources;
        uint64_t m_nextEventId{0};
        SimTime m_currentSimTime;

        SimEngineState m_simEngineState;

        std::unordered_map<UUID, Net> m_nets;

        std::vector<std::shared_ptr<Drivers::SimDriver>> m_simDrivers;
        std::vector<std::thread> m_driverThreads;

        typedef std::unordered_map<UUID, std::shared_ptr<Drivers::SimDriver>> CompDriverMap;

        bool m_destroyed{false};

        bool m_isNetUpdated{false};
        bool m_isSimulating{false};
    };
} // namespace Bess::SimEngine
