#pragma once

#include "common/bess_api.h"
#include "common/class_helpers.h"
#include "common/types.h"
#include "net/net.h"
#include "simulation_clock.h"
#include <common/bess_uuid.h>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace Bess::SimEngine {
    class SimulationEngine;
}

namespace Bess::SimEngine::Drivers {

    class BESS_API SimFnDataBase {
      public:
        virtual ~SimFnDataBase() = default;
        bool simDependants = false;
    };

    class BESS_API CompDef {
      public:
        typedef std::shared_ptr<SimFnDataBase> SimFnDataPtr;
        typedef std::function<SimFnDataPtr(const SimFnDataPtr &)> SimFn;

        CompDef() = default;
        virtual ~CompDef() = default;

        MAKE_GETTER_SETTER(std::string, Name, m_name)
        MAKE_GETTER_SETTER(std::string, GroupName, m_groupName)
        MAKE_VGETTER_VSETTER(SimFn, SimFn, m_simFn)
        MAKE_GETTER_SETTER(ComponentBehaviorType, BehaviorType, m_behaviorType)

        virtual Json::Value toJson() const;

        virtual void loadJson(const Json::Value &json);

        virtual std::shared_ptr<CompDef> clone() const = 0;

        virtual std::string getTypeName() const = 0;

        virtual PortDescriptor getInputPortDescriptor() const;

        virtual PortDescriptor getOutputPortDescriptor() const;

      protected:
        std::string m_name;
        std::string m_groupName;
        SimFn m_simFn = nullptr;
        ComponentBehaviorType m_behaviorType = ComponentBehaviorType::none;
    };

    class BESS_API SimComponent {
      public:
        SimComponent() = default;
        virtual ~SimComponent() = default;

        template <typename TCompDef>
        std::shared_ptr<TCompDef> getDefinition() const {
            return std::dynamic_pointer_cast<TCompDef>(m_def);
        }

        MAKE_GETTER_SETTER(UUID, Uuid, m_uuid)
        MAKE_GETTER_SETTER(std::string, Name, m_name)
        MAKE_GETTER_SETTER(std::shared_ptr<CompDef>, Definition, m_def)

        virtual Json::Value toJson() const;
        virtual void loadJson(const Json::Value &json);

        virtual void onPostSimulate() {
        }

        virtual std::shared_ptr<SimFnDataBase>
        simulate(const std::shared_ptr<SimFnDataBase> &data);

      protected:
        UUID m_uuid; // will auto gen id for each instance
        std::string m_name;
        std::shared_ptr<CompDef> m_def = nullptr;
    };

    enum class SimDriverState : uint8_t {
        uninitialized,
        destroyed,
        stopped,
        running,
        paused
    };

    // comp id, direction, signal kind, new count
    typedef std::function<void(const UUID &, PortDirection, SignalKind, int)>
        PortCountChangeCB;

    struct BESS_API PortCountChangeRes {
        bool changedInputs = false;
        bool changedOutputs = false;

        static PortCountChangeRes noChange() {
            return PortCountChangeRes{false, false};
        }

        static PortCountChangeRes inputsChanged() {
            return PortCountChangeRes{true, false};
        }

        static PortCountChangeRes outputsChanged() {
            return PortCountChangeRes{false, true};
        }

        static PortCountChangeRes bothChanged() {
            return PortCountChangeRes{true, true};
        }

        bool hasChange() const {
            return changedInputs || changedOutputs;
        }
    };

    class BESS_API SimDriver {
      public:
        struct ComponentStamp {
            TimeNs simTime{0};
            std::vector<PortState> inputStates;
            std::vector<PortState> outputStates;
        };

        // Transition-compressed during a normal run and uniformly sampled
        // during a timed run.
        using CompStampData = NodeHashMap<UUID, std::vector<ComponentStamp>>;
        using SchedulerNotifyFn = std::function<void()>;
        static constexpr std::size_t MaxStampSamplesPerComponent = 40000;

        SimDriver();
        virtual ~SimDriver() = default;

        // will be ran in seperate thread
        virtual void run() = 0;

        virtual std::string getName() const = 0;

        // returns whether driver will accept the component.
        virtual bool supportsDef(const std::shared_ptr<CompDef> &def) const = 0;

        virtual std::shared_ptr<SimComponent>
        createComp(const std::shared_ptr<CompDef> &def,
                   bool cloneDef = true) = 0;

        virtual UUID addComponent(const std::shared_ptr<SimComponent> &comp,
                                  bool scheduleSim);

        virtual void deleteComponent(const UUID &uuid);

        virtual void clearComponents();

        // Compatibility hook for existing drivers. New drivers get complete
        // stamping from the TimeNs overload without implementing anything.
        virtual void stampSim(TimeMs /*elapsedTime*/) {
        }

        // Capture a stable state snapshot. When includeUnchanged is false the
        // history is transition-compressed.
        virtual void stampSim(TimeNs simTime, bool includeUnchanged = false);

        [[nodiscard]] CompStampData getStampData() const;
        void clearStampData();

        // Engine-coordinated scheduling contract. New scheduled drivers
        // should opt in and implement the next/process pair; deriving from
        // EvtBasedSimDriver provides that implementation. These defaults keep
        // older independent run-loop drivers source-compatible.
        virtual bool usesGlobalClockScheduling() const {
            return false;
        }
        virtual std::optional<TimeNs> getNextEventTime() const {
            return std::nullopt;
        }
        virtual size_t processEventsAt(TimeNs /*simTime*/) {
            return 0;
        }

        bool beginRun(TimeNs startTime = TimeNs(0));

        virtual bool isSimStable() const;

        virtual void clearPendingEvents() {
        }

        // Connection management
        virtual std::pair<bool, std::string>
        canConnectPorts(const PortRef &src, const PortRef &dst) const = 0;

        virtual bool connectPorts(const PortRef &src,
                                  const PortRef &dst,
                                  bool overrideConn) = 0;

        virtual void deleteConnection(const PortRef &portA,
                                      const PortRef &portB) = 0;

        virtual PortCountChangeRes addPort(const PortRef &port,
                                           bool force = false) = 0;

        virtual PortCountChangeRes removePort(const PortRef &port,
                                              bool force = false) = 0;

        virtual ConnectionBundle getConnections(const UUID &uuid) const;

        virtual std::vector<PortState> getInputPortStates(const UUID &compId);

        virtual PortState getPortState(const PortRef &port) const;

        virtual bool
        setInputPortState(const UUID &uuid, int pinIdx, const PortState &state);

        virtual bool setOutputPortState(const UUID &uuid,
                                        int pinIdx,
                                        const PortState &state);

        virtual ComponentState getComponentState(const UUID &uuid) const;

        virtual void propagateFromComponent(const UUID &sourceId);

        virtual const std::unordered_map<UUID, Net> &getNetsMap() const;

        virtual bool isNetUpdated() const;

        virtual void clearNetUpdated();

        virtual Json::Value toJson() const = 0;

        virtual void loadJson(const Json::Value &json) = 0;

        void addOnPortCountChangeCB(const UUID &id,
                                    const PortCountChangeCB &cb);

        void removeOnPortCountChangeCB(const UUID &id);

      protected:
        virtual void
        onComponentAdded(const std::shared_ptr<SimComponent> & /*comp*/) {
        }

        virtual void onInit() {};

        virtual void onStop() {};

        virtual void onReset() {};

        virtual void onPause() {};

        virtual void onResume() {};

        virtual void onStep() {};

        virtual void onDestroy() {};

        void triggerPortCountChangeCbs(const UUID &compId,
                                       PortDirection direction,
                                       SignalKind signalKind,
                                       int newCount);

      public:
        typedef std::shared_ptr<SimComponent> SimComponentPtr;
        typedef HashMap<UUID, SimComponentPtr> ComponentsMap;

        MAKE_GETTER_SETTER_MT(ComponentsMap,
                              ComponentsMap,
                              m_components,
                              m_compMapMutex)

        MAKE_GETTER_SETTER_MT(SimDriverState, State, m_state, m_stateMutex)

        bool hasComponent(const UUID &id) const;

        template <typename TComp>
        std::shared_ptr<TComp> getComponentSP(const UUID &id) const {
            std::lock_guard lk(m_compMapMutex);

            auto it = m_components.find(id);
            if (it == m_components.end()) {
                return nullptr;
            }
            return std::dynamic_pointer_cast<TComp>(it->second);
        }

        // For hotpaths
        template <typename TComp> TComp *getComponent(const UUID &id) const {
            std::lock_guard lk(m_compMapMutex);

            auto it = m_components.find(id);
            if (it == m_components.end()) {
                return nullptr;
            }
            return static_cast<TComp *>(it->second.get());
        }

        void init();

        bool isInitialized() const;

        bool isRunning() const;

        bool isPaused() const;

        bool isStopped() const;

        bool isDestroyed() const;

        void pause();

        void resume();

        void stop();

        void reset();

        void destroy();

        void step();

        void setEngine(Bess::SimEngine::SimulationEngine *engine) {
            m_engine = engine;
        }

        [[nodiscard]] Bess::SimEngine::SimulationEngine *getEngine() const {
            return m_engine;
        }

        void setSimulationClock(
            const std::shared_ptr<Bess::SimEngine::SimulationClock> &clock);

        [[nodiscard]] std::shared_ptr<Bess::SimEngine::SimulationClock>
        getSimulationClock() const;

        [[nodiscard]] TimeNs getCurrentSimTime() const;

        void setSchedulerNotifyFn(SchedulerNotifyFn notifyFn);

      protected:
        virtual void onRunStart(TimeNs /*startTime*/) {
        }

        void notifyScheduler();

        ComponentsMap m_components;
        SimDriverState m_state = SimDriverState::uninitialized;
        mutable std::mutex m_compMapMutex;
        mutable std::mutex m_stateMutex;

        CompStampData m_compStampData;
        HashSet<UUID> m_truncatedStampComponents;
        mutable std::mutex m_stampMutex;
        std::shared_ptr<Bess::SimEngine::SimulationClock> m_simulationClock;
        SchedulerNotifyFn m_schedulerNotifyFn;
        mutable std::mutex m_schedulerNotifyMutex;
        std::unordered_map<UUID, PortCountChangeCB> m_onPortCountChangeCBs;
        Bess::SimEngine::SimulationEngine *m_engine = nullptr;
    };
} // namespace Bess::SimEngine::Drivers
