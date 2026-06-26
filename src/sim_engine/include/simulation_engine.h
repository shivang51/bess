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

    class BESS_API SimulationEngine : public ISubSystem {
      public:
        SimulationEngine();
        ~SimulationEngine() override;

        void onInit() override;
        void onDestroy() override;
        void onPostInit() override;

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

        bool connectComponent(const UUID &src,
                              int srcSlotIdx,
                              SlotType srcType,
                              const UUID &dst,
                              int dstSlotIdx,
                              SlotType dstType,
                              bool overrideConn = false);

        // returns {canConnect, errorMessage}
        std::pair<bool, std::string>
        canConnectComponents(const UUID &src,
                             int srcSlotIdx,
                             SlotType srcType,
                             const UUID &dst,
                             int dstSlotIdx,
                             SlotType dstType) const;

        void deleteComponent(const UUID &uuid);

        void deleteConnection(const UUID &compA,
                              SlotType pinAType,
                              int idxA,
                              const UUID &compB,
                              SlotType pinBType,
                              int idxB);

        SlotState getDigitalSlotState(const UUID &uuid, SlotType type, int idx);

        ConnectionBundle getConnections(const UUID &uuid);
        std::vector<SlotState> getInputSlotsState(UUID compId) const;

        void setInputSlotState(const UUID &uuid, int pinIdx, LogicState state);
        void setOutputSlotState(const UUID &uuid, int pinIdx, LogicState state);

        SimulationState toggleSimState();
        SimulationState getSimulationState() const;
        void setSimulationState(SimulationState state);
        void clearPendingDriverEvents();

        // only steps if sim state is paused
        void stepSimulation();

        const ComponentState &getComponentState(const UUID &uuid);
        const std::shared_ptr<Drivers::CompDef> &
        getComponentDefinition(const UUID &uuid) const;

        void clear();

        bool addSlot(const UUID &compId,
                     SlotType type,
                     int index,
                     bool force = false);
        bool removeSlot(const UUID &compId,
                        SlotType type,
                        int index,
                        bool force = false);

        friend class SimEngineSerializer;

        bool isNetUpdated() const;

        // if update is false, the sync flag will not be reset
        std::unordered_map<UUID, Net> getNetsMap(bool update = true);

        void triggerPropagation(const UUID &sourceId);
        void markPendingSignalSource(const UUID &sourceId);

        bool isSimStable();

        void addOnSlotCountChangeCB(const UUID &id,
                                    const Drivers::SlotCountChangeCB &cb);

        void removeOnSlotCountChangeCB(const UUID &id);

        std::shared_ptr<Drivers::SimDriver>
        getDriverWithName(const std::string &name) const;

        const std::vector<std::shared_ptr<Drivers::SimDriver>> &
        getDrivers() const;

        Json::Value toJson() const;
        void loadJson(const Json::Value &json);

      private:
        void loadDrivers();
        void unloadDrivers();

        void initDrivers();
        void destroyDrivers();

        void runDrivers();
        void stopDrivers();

      private:
        void propagateFromComponent(const UUID &sourceId);
        void processPendingPropagation();

        void run();

        mutable std::mutex m_stateMutex;
        mutable std::mutex m_driversMutex;
        mutable std::mutex m_pendingSignalSourcesMutex;

        std::atomic<bool> m_stepFlag{false};
        std::atomic<SimulationState> m_simState{SimulationState::running};
        std::condition_variable m_stateCV;

        std::set<UUID> m_pendingSignalSources;

        std::vector<std::shared_ptr<Drivers::SimDriver>> m_simDrivers;
        std::vector<std::thread> m_driverThreads;

        bool m_destroyed{false};

        bool m_isSimulating{false};
    };
} // namespace Bess::SimEngine
