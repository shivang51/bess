#pragma once

#include "bess_api.h"
#include "common/bess_assert.h"
#include "common/bess_uuid.h"
#include "common/logger.h"
#include "common/types.h"
#include "sim_driver.h"
#include "types.h"
#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

namespace Bess::SimEngine::Drivers {

    class BESS_API EvtBasedCompDef : public CompDef {
      public:
        EvtBasedCompDef() = default;
        ~EvtBasedCompDef() override = default;

        MAKE_GETTER_SETTER(bool, AutoReschedule, m_autoReschedule)
        MAKE_GETTER_SETTER(TimeNs, PropDelay, m_propDelay)

        Json::Value toJson() const override;

        static void fromJson(const std::shared_ptr<EvtBasedCompDef> &compDef,
                             const Json::Value &json) {}

        virtual TimeNs getSelfSimDelay();

      protected:
        bool m_autoReschedule = false;
        TimeNs m_propDelay{0}; // propogation delay
    };

    class BESS_API EvtBasedSimComp : public SimComponent {
      public:
        EvtBasedSimComp() = default;
        ~EvtBasedSimComp() override = default;

        TimeNs getPropDelay() const;

        bool getSimSelf() const;

        TimeNs getSelfSimDelay() const;

        Json::Value toJson() const override;

        static void fromJson(const std::shared_ptr<EvtBasedSimComp> &comp,
                             const Json::Value &json);
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

        // true if the dependants should be simulated, false otherwise
        virtual bool simulate(const SimEvt &evt) = 0;

        virtual void addComponent(
            const std::shared_ptr<EvtBasedSimComp> &comp,
            bool scheduleSim = true);

        virtual std::vector<UUID> getDependants(const UUID &id);

        virtual std::vector<SlotState> collapseInputs(const UUID &id);

        virtual void onBeforeRun();

        void onPause() override;

        void onResume() override;

        void onStop() override;

        void scheduleEvt(const UUID &compId,
                         TimeNs simTime,
                         const UUID &schedulerId,
                         bool notify = true);

        void clearPendingEvents() override;

      private:
        void simulateEvts(const std::vector<SimEvt> &evts);

        void scheduleDependantsOf(const UUID &compId);

        // returns the next evt baesd on sim time
        SimEvt getNextEvt() const;

        std::vector<SimEvt> collectEvts();

      protected:
        TimeNs m_currentSimTime{0};
        std::condition_variable m_runIterCv;
        std::mutex m_runIterMutex;

      private:
        std::set<SimEvt> m_events;
    };

} // namespace Bess::SimEngine::Drivers
