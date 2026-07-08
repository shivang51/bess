#pragma once

#include "common/bess_api.h"
#include "common/bess_uuid.h"
#include "common/sub_system.h"
#include "common/types.h"
#include "sim_driver/sim_driver.h"
#include <condition_variable>
#include <memory>
#include <mutex>
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

        MAKE_GETTER_SETTER(SimRunCtx, RunCtx, m_runCtx);

        void destroy();

        const UUID &
        addComponent(const std::shared_ptr<Drivers::CompDef> &definition,
                     bool cloneDef = true);

        template <typename T>
        std::shared_ptr<T> getComponentSP(const UUID &uuid) const {
            for (const auto &driver : m_simDrivers) {
                auto comp = driver->template getComponentSP<T>(uuid);
                if (comp) {
                    return comp;
                }
            }

            return nullptr;
        }

        template <typename T> T *getComponent(const UUID &uuid) const {
            for (const auto &driver : m_simDrivers) {
                auto comp = driver->template getComponent<T>(uuid);
                if (comp) {
                    return comp;
                }
            }

            return nullptr;
        }

        bool connectPorts(const PortRef &src,
                          const PortRef &dst,
                          bool overrideConn = false);

        // returns {canConnect, errorMessage}
        std::pair<bool, std::string>
        canConnectPorts(const PortRef &src, const PortRef &dst) const;

        void deleteComponent(const UUID &uuid);

        void deleteConnection(const PortRef &portA, const PortRef &portB);

        SlotState getPortState(const PortRef &port);

        ConnectionBundle getConnections(const UUID &uuid);
        std::vector<SlotState> getInputSlotsState(UUID compId) const;

        void setInputSlotState(const UUID &uuid, int pinIdx, LogicState state);
        void setOutputSlotState(const UUID &uuid, int pinIdx, LogicState state);

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

        void run();
        void runFor(TimeMs duration, TimeMs stepInterval = TimeMs(0));
        void stop();

      private:
        void loadDrivers();
        void unloadDrivers();

        void initDrivers();
        void destroyDrivers();

        void runDrivers();
        void stopDrivers();
        void pauseDrivers();
        void resumeDrivers();

        // Stamps state of each component at this time
        void stampSim(TimeMs elapsedTime);

      private:
        void propagateFromComponent(const UUID &sourceId);
        void processPendingPropagation();

        mutable std::mutex m_stateMutex;
        mutable std::mutex m_driversMutex;
        mutable std::mutex m_pendingSignalSourcesMutex;

        std::atomic<bool> m_stepFlag{false};
        std::atomic<SimulationState> m_simState{SimulationState::stopped};
        std::condition_variable m_stateCV;

        std::set<UUID> m_pendingSignalSources;

        std::vector<std::shared_ptr<Drivers::SimDriver>> m_simDrivers;
        std::vector<std::thread> m_driverThreads;
        std::thread m_timedRunThread;

        SimRunCtx m_runCtx;

        bool m_destroyed{false};
    };
} // namespace Bess::SimEngine
