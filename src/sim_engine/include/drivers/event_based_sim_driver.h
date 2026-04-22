#pragma once

#include "bess_api.h"
#include "common/bess_assert.h"
#include "common/bess_uuid.h"
#include "common/logger.h"
#include "common/types.h"
#include "sim_driver.h"
#include "types.h"
#include <condition_variable>
#include <set>

namespace Bess::SimEngine {

    template <typename TSimFnData>
    class BESS_API EvtBasedSimComponent : public SimComponent<TSimFnData> {
      public:
        EvtBasedSimComponent() = default;
        ~EvtBasedSimComponent() override = default;

        MAKE_GETTER_SETTER(TimeNs, PropDelay, m_propogationDelay)
        MAKE_GETTER_SETTER(bool, SimSelf, m_simSelf)

        virtual TimeNs getSelfSimDelay() {
            return TimeNs(0);
        }

        Json::Value toJson() const override {
            Json::Value json = SimComponent<TSimFnData>::toJson();
            json["propagationDelay"] = m_propogationDelay.count();
            json["simSelf"] = m_simSelf;
            return json;
        }

        static void fromJson(const std::shared_ptr<EvtBasedSimComponent> &comp,
                             const Json::Value &json);

      private:
        TimeNs m_propogationDelay{0};
        bool m_simSelf = false;
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
    };

    template <typename TSimFnData>
    class BESS_API EvtBasedSimDriver : public SimDriver<TSimFnData> {
      public:
        EvtBasedSimDriver() = default;
        ~EvtBasedSimDriver() override = default;

        void run() override {
            onBeforeRun();

            BESS_ASSERT(this->isInitialized(),
                        "SimDriver must be initialized before running");
            BESS_ASSERT(this->isDestroyed(),
                        "SimDriver was destroyed, cannot run");

            while (!this->isStopped()) {
                std::unique_lock lk(m_runIterCv);
                m_runIterCv.wait(lk, [&] {
                    return this->isStopped() || !this->getComponentsMap().empty();
                });

                if (this->isStopped()) {
                    break;
                }

                const auto evtsToSim = collectEvts();
                if (evtsToSim.empty()) {
                    const auto &nextEvt = getNextEvt();
                    std::this_thread::sleep_for(nextEvt.simTime - m_currentSimTime);
                    m_currentSimTime = nextEvt.simTime;
                    continue;
                }

                simulateEvts(evtsToSim);
            }
        }

        // true if the dependants should be simulated, false otherwise
        virtual bool simulate(const UUID &id) = 0;

        virtual std::vector<UUID> getDependants(const UUID &id) {
            return {};
        }

        virtual std::vector<SlotState> collapseInputs(const UUID &id) {
            return {};
        }

        virtual void onBeforeRun() {}

      private:
        void simulateEvts(const std::vector<SimEvt> &evts) {
            for (const auto &evt : evts) {
                const bool simDependants = simulate(evt.compId);

                const auto &comp = this->template getComponent<EvtBasedSimDriver>(
                    evt.compId);

                if (!comp) {
                    BESS_WARN("(EvtBasedSimDriver.run) Component with UUID {} not found",
                              (uint64_t)evt.compId);
                    continue;
                }

                scheduleEvt(evt.compId,
                            evt.simTime + comp->getPropDelay(),
                            evt.schedulerId);

                if (simDependants) {
                    scheduleDependantsOf(evt.compId);
                }

                if (comp->getSimSelf()) {
                    scheduleEvt(evt.compId,
                                evt.simTime + comp->getSelfSimDelay(),
                                evt.schedulerId);
                }
            }
        }

        void scheduleDependantsOf(const UUID &compId) {
            const auto &comp = this->template getComponent<EvtBasedSimDriver>(
                compId);

            if (!comp) {
                BESS_WARN("(EvtBasedSimDriver.scheduleDependantsOf) Component with UUID {} not found",
                          (uint64_t)compId);
                return;
            }

            const auto dependants = getDependants(compId);

            for (const auto &id : dependants) {
                scheduleEvt(id,
                            m_currentSimTime + comp->getPropDelay(),
                            compId);
            }
        }

        // returns the next evt baesd on sim time
        SimEvt getNextEvt() const {
            SimEvt ev;
            if (m_events.empty()) {
                // if no events so return with currentSimTime,
                // so it just continues to wait for new events to be scheduled
                ev.simTime = m_currentSimTime;
                return ev;
            }

            ev.simTime = TimeNs::max();

            for (const auto &evt : m_events) {
                if (evt.simTime < ev.simTime) {
                    ev = evt;
                }
            }

            return ev;
        }

        void scheduleEvt(const UUID &compId,
                         TimeNs simTime,
                         const UUID &schedulerId) {
            static uint64_t evtId = 0;
            SimEvt ev{UUID(evtId++),
                      compId,
                      schedulerId,
                      simTime};
            m_events.insert(ev);
        }

        std::vector<SimEvt> collectEvts() {
            std::set<UUID> collectedCompIds = {};
            std::vector<SimEvt> evtsToSim = {};

            for (const auto &evt : m_events) {
                if (evt.simTime <= m_currentSimTime) {
                    continue;
                }

                if (collectedCompIds.contains(evt.compId)) {
                    continue;
                }

                collectedCompIds.insert(evt.compId);
                evtsToSim.push_back(evt);
            }

            // sort by sim time, then by event id to ensure deterministic order
            std::sort(evtsToSim.begin(),
                      evtsToSim.end(),
                      [](const SimEvt &a, const SimEvt &b) {
                          if (a.simTime != b.simTime) {
                              return a.simTime < b.simTime;
                          }
                          return a.evtId < b.evtId;
                      });

            // remove collected evts from the event set
            for (const auto &evt : evtsToSim) {
                m_events.erase(evt);
            }

            return evtsToSim;
        }

      protected:
        TimeNs m_currentSimTime{0};

      private:
        std::set<SimEvt> m_events;
        std::condition_variable m_runIterCv;
    };

} // namespace Bess::SimEngine
