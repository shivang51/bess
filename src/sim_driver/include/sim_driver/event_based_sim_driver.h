#pragma once

#include "common/bess_api.h"

#include "common/bess_uuid.h"
#include "common/types.h"
#include "sim_driver.h"
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <set>
#include <vector>

namespace Bess::SimEngine::Drivers {

    class BESS_API EvtBasedCompDef : public CompDef {
      public:
        EvtBasedCompDef() = default;
        ~EvtBasedCompDef() override = default;

        MAKE_GETTER_SETTER(bool, AutoReschedule, m_autoReschedule)
        MAKE_GETTER_SETTER(TimeNs, PropDelay, m_propDelay)
        MAKE_GETTER_SETTER(TimeNs, AutoRescheduleDelay, m_autoRescheduleDelay)

        Json::Value toJson() const override;

        void loadJson(const Json::Value &json) override;

        // Delay between run start and the component's first self-simulation.
        // Most computed sources need to settle at time zero, so the default is
        // zero. Periodic sources can override this to preserve their initial
        // phase for a non-zero interval.
        virtual TimeNs getInitialSimDelay();

        virtual TimeNs getSelfSimDelay();

        // Called after a self-simulation has completed. The default preserves
        // the fixed-delay behavior; phase-dependent sources may vary the next
        // interval deterministically without storing runtime state in their
        // reusable definition.
        virtual TimeNs getSelfSimDelayAfter(uint64_t completedSelfSimulations);

      protected:
        bool m_autoReschedule = false;
        TimeNs m_propDelay{0};           // propogation delay
        TimeNs m_autoRescheduleDelay{0}; // self simulation delay
    };

    class BESS_API EvtBasedSimComp : public SimComponent {
      public:
        // void(input states, output states)
        typedef std::function<void(const std::vector<PortState> &,
                                   const std::vector<PortState> &)>
            TOnStateChangeFn;

        EvtBasedSimComp() = default;
        ~EvtBasedSimComp() override = default;

        TimeNs getPropDelay() const;

        bool getSimSelf() const;

        TimeNs getSelfSimDelay() const;

        TimeNs getInitialSimDelay() const;

        void recordSelfSimulation();

        Json::Value toJson() const override;

        void loadJson(const Json::Value &json) override;

        void resetRuntimeState(TimeNs startTime) override;

        void addOnStateChangeCB(const UUID &id, const TOnStateChangeFn &cb);

        void removeOnStateChangeCB(const UUID &id);

        typedef std::unordered_map<UUID, TOnStateChangeFn> TOnStateChangeCbsMap;
        MAKE_GETTER(TOnStateChangeCbsMap, OnStateChangeCbs, m_onStateChangeCbs);

      private:
        TOnStateChangeCbsMap m_onStateChangeCbs;
        uint64_t m_completedSelfSimulations{0};
    };

    struct BESS_API SimEvt {
        UUID evtId;
        UUID compId;
        UUID schedulerId;
        TimeNs simTime;
        bool operator<(const SimEvt &other) const noexcept {
            if (simTime != other.simTime)
                return simTime < other.simTime;
            return evtId < other.evtId;
        }

        bool operator==(const SimEvt &other) const noexcept {
            return evtId == other.evtId;
        }
    };

    class BESS_API EvtBasedSimDriver : public SimDriver {
      public:
        EvtBasedSimDriver() = default;
        ~EvtBasedSimDriver() override = default;

        void run() override;

        bool usesGlobalClockScheduling() const override {
            return true;
        }

        std::optional<TimeNs> getNextEventTime() const override;

        size_t processEventsAt(TimeNs simTime) override;

        bool isSimStable() const override;

        // true if the dependants should be simulated, false otherwise
        virtual bool simulate(const SimEvt &evt,
                              const std::vector<PortState> &inputs) = 0;

        UUID addComponent(const std::shared_ptr<SimComponent> &comp,
                          bool scheduleSim) override;

        void deleteComponent(const UUID &uuid) override;

        void clearComponents() override;

        virtual std::vector<UUID> getDependants(const UUID &id);

        virtual std::vector<PortState> collapseInputs(const UUID &id);

        void propagateFromComponent(const UUID &sourceId) override;

        virtual void onBeforeRun();

        void onPause() override;

        void onResume() override;

        void onStop() override;

        void onStep() override;

        void scheduleEvt(const UUID &compId,
                         TimeNs simTime,
                         const UUID &schedulerId,
                         bool notify = true);

        void clearPendingEvents() override;

      private:
        TimeNs
        initialEventTime(const std::shared_ptr<EvtBasedSimComp> &component,
                         TimeNs startTime) const;

        void prepareRunStartLocked(TimeNs startTime);

        void scheduleDeferredRunStartEvents();

        void simulateEvts(const std::vector<SimEvt> &evts, TimeNs simTime);

        void scheduleEvtLocked(const UUID &compId,
                               TimeNs simTime,
                               const UUID &schedulerId);

        void scheduleDependantsOfAt(const UUID &compId, TimeNs simTime);

        std::vector<SimEvt> collectEvtsAt(TimeNs simTime);

      protected:
        void onRunStart(TimeNs startTime) override;

        void scheduleDependantsOf(const UUID &compId);

        std::condition_variable m_runIterCv;
        mutable std::mutex m_eventMutex;
        mutable std::mutex m_processMutex;
        std::set<SimEvt> m_events;
        HashSet<UUID> m_runStartScheduledCompIds;
        std::vector<UUID> m_deferredRunStartCompIds;
        uint64_t m_nextEventId{1};
        size_t m_eventsInFlight{0};
    };

} // namespace Bess::SimEngine::Drivers
