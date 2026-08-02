#pragma once

#include "common/bess_api.h"
#include "common/bess_uuid.h"
#include "common/sub_system.h"
#include "common/types.h"
#include "sim_driver/sim_driver.h"
#include "sim_driver/simulation_clock.h"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <thread>

namespace Bess::SimEngine {
    class ComponentDefinition;
    namespace Drivers::Digital {
        class DigSimComp;
    } // namespace Drivers::Digital

    struct BESS_API SimRunCtx {
        TimeMs runDuration{0};
        TimeMs stepInterval{0};
        TimeMs elapsedTime{0};
        bool isTimedRun{false};
        SimulationState simState{SimulationState::stopped};

        bool isSimulating() const {
            return simState == SimulationState::running;
        }

        bool isPaused() const {
            return simState == SimulationState::paused;
        }

        bool isStopped() const {
            return simState == SimulationState::stopped;
        }
    };

    class BESS_API SimulationEngine : public ISubSystem {
      public:
        SimulationEngine();
        ~SimulationEngine() override;

        void onInit() override;
        void onDestroy() override;
        void onPostInit() override;

        [[nodiscard]] SimRunCtx getRunCtx() const;
        void setRunCtx(const SimRunCtx &runCtx);

        [[nodiscard]] TimeNs getCurrentSimTime() const;

        [[nodiscard]] Drivers::SimDriver::CompStampData getStampData() const;
        void clearStampData();

        void destroy();

        const UUID &
        addComponent(const std::shared_ptr<Drivers::CompDef> &definition,
                     bool cloneDef = true);

        template <typename T>
        std::shared_ptr<T> getComponentSP(const UUID &uuid) const {
            std::lock_guard executionLock(m_schedulerExecutionMutex);
            for (const auto &driver : m_simDrivers) {
                auto comp = driver->template getComponentSP<T>(uuid);
                if (comp) {
                    return comp;
                }
            }

            return nullptr;
        }

        template <typename T> T *getComponent(const UUID &uuid) const {
            std::lock_guard executionLock(m_schedulerExecutionMutex);
            for (const auto &driver : m_simDrivers) {
                auto comp = driver->template getComponentSP<T>(uuid);
                if (comp) {
                    return comp.get();
                }
            }

            return nullptr;
        }

        bool connectPorts(const PortRef &src,
                          const PortRef &dst,
                          bool overrideConn = false);

        // returns {canConnect, errorMessage}
        std::pair<bool, std::string> canConnectPorts(const PortRef &src,
                                                     const PortRef &dst) const;

        void deleteComponent(const UUID &uuid);

        void deleteConnection(const PortRef &portA, const PortRef &portB);

        PortState getPortState(const PortRef &port);

        ConnectionBundle getConnections(const UUID &uuid);
        std::vector<PortState> getInputPortStates(UUID compId) const;

        void
        setInputPortState(const UUID &uuid, int pinIdx, const PortState &state);
        void setOutputPortState(const UUID &uuid,
                                int pinIdx,
                                const PortState &state);

        void setInputPortState(const UUID &uuid, int pinIdx, LogicState state);

        void setOutputPortState(const UUID &uuid, int pinIdx, LogicState state);

        SimulationState toggleStartStop();
        SimulationState togglePlayPause();
        SimulationState getSimulationState() const;
        void setSimulationState(SimulationState state);
        void clearPendingDriverEvents();

        // only steps if sim state is paused
        void stepSimulation();

        const ComponentState &getComponentState(const UUID &uuid);
        const std::shared_ptr<Drivers::CompDef> &
        getComponentDefinition(const UUID &uuid) const;

        // Clears all components and connections from the simulation engine.
        // If restoreState is true, the simulation engine will be restored to
        // its initial sim state after clearing otherwise it will be stopped.
        void clear(bool restoreState = true);

        bool addPort(const PortRef &port, bool force = false);
        bool removePort(const PortRef &port, bool force = false);

        friend class SimEngineSerializer;

        bool isNetUpdated() const;

        // if update is false, the sync flag will not be reset
        std::unordered_map<UUID, Net> getNetsMap(bool update = true);

        void triggerPropagation(const UUID &sourceId);
        void markPendingSignalSource(const UUID &sourceId);

        bool isSimStable();

        void addOnPortCountChangeCB(const UUID &id,
                                    const Drivers::PortCountChangeCB &cb);

        void removeOnPortCountChangeCB(const UUID &id);

        std::shared_ptr<Drivers::SimDriver>
        getDriverWithName(const std::string &name) const;

        const std::vector<std::shared_ptr<Drivers::SimDriver>> &
        getDrivers() const;

        Json::Value toJson() const;
        void loadJson(const Json::Value &json);

        // Run continuously with virtual time paced against steady wall time.
        void run();

        // Run an unpaced, deterministic transient simulation. stepInterval
        // controls waveform sampling; zero records only the settled initial
        // and final states.
        void runFor(TimeMs duration, TimeMs stepInterval = TimeMs(0));
        void stop();

      private:
        struct SchedulerAction {
            TimeNs time{0};
            bool sample = false;
            bool final = false;
        };

        void loadDrivers();
        void unloadDrivers();

        void initDrivers();
        void destroyDrivers();

        [[nodiscard]] bool runDrivers();
        void requestDriverStops();
        void stopDrivers();
        void pauseDrivers();
        void resumeDrivers();

        void startRun(bool timedRun, TimeNs duration, TimeNs sampleInterval);
        void requestStop();
        void stopLocked();
        void schedulerLoop(uint64_t generation, bool realTimePaced);
        [[nodiscard]] std::optional<TimeNs> getNextGlobalEventTime();
        [[nodiscard]] std::optional<SchedulerAction> getNextSchedulerAction();
        [[nodiscard]] bool
        executeSchedulerAction(const SchedulerAction &action);
        [[nodiscard]] bool settleDriversAt(TimeNs simTime);
        void completeRun(uint64_t generation);
        void notifyScheduler();
        void updateElapsedTime(TimeNs simTime);
        void advanceSampleTime(TimeNs processedTime);

        // Capture a stable snapshot from every driver at this global time.
        void stampSim(TimeNs simTime, bool includeUnchanged);

      private:
        void propagateFromComponent(const UUID &sourceId);
        void processPendingPropagation();

        mutable std::mutex m_stateMutex;
        mutable std::mutex m_lifecycleMutex;
        mutable std::recursive_mutex m_schedulerExecutionMutex;
        mutable std::mutex m_driverThreadMutex;
        mutable std::mutex m_driversMutex;
        mutable std::mutex m_pendingSignalSourcesMutex;

        std::atomic<bool> m_stepFlag{false};
        std::atomic<SimulationState> m_simState{SimulationState::stopped};
        std::condition_variable m_stateCV;
        uint64_t m_schedulerRevision{0};
        std::atomic<uint64_t> m_runGeneration{0};

        std::set<UUID> m_pendingSignalSources;

        std::vector<std::shared_ptr<Drivers::SimDriver>> m_simDrivers;
        std::vector<std::thread> m_legacyDriverThreads;
        std::thread m_schedulerThread;

        std::shared_ptr<SimulationClock> m_simulationClock;

        TimeNs m_runEndTime{0};
        TimeNs m_sampleInterval{0};
        TimeNs m_nextSampleTime{0};
        bool m_initialSamplePending{false};

        SimRunCtx m_runCtx;

        bool m_destroyed{false};
    };
} // namespace Bess::SimEngine
