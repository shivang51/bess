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

    class BESS_API EvtBasedCompDef : public ComponentDef {
      public:
        EvtBasedCompDef() = default;
        ~EvtBasedCompDef() override = default;

        MAKE_GETTER_SETTER(bool, AutoReschedule, m_autoReschedule)
        MAKE_GETTER_SETTER(TimeNs, PropDelay, m_propDelay)

        Json::Value toJson() const override {
            Json::Value json = ComponentDef::toJson();
            json["shouldAutoReschedule"] = m_autoReschedule;
            json["propDelay"] = m_propDelay.count();
            return json;
        }

        static void fromJson(const std::shared_ptr<EvtBasedCompDef> &compDef,
                             const Json::Value &json) {}

        virtual TimeNs getSelfSimDelay() {
            return TimeNs(0);
        }

        std::shared_ptr<ComponentDef> clone() const override {
            return std::make_shared<EvtBasedCompDef>(*this);
        }

      protected:
        bool m_autoReschedule = false;
        TimeNs m_propDelay{0}; // propogation delay
    };

    class BESS_API EvtBasedSimComponent : public SimComponent {
      public:
        EvtBasedSimComponent() = default;
        ~EvtBasedSimComponent() override = default;

        TimeNs getPropDelay() const {
            auto def = std::dynamic_pointer_cast<EvtBasedCompDef>(m_def);
            return def ? def->getPropDelay() : TimeNs(0);
        }

        bool getSimSelf() const {
            auto def = std::dynamic_pointer_cast<EvtBasedCompDef>(m_def);
            return def ? def->getAutoReschedule() : false;
        }

        TimeNs getSelfSimDelay() const {
            auto def = std::dynamic_pointer_cast<EvtBasedCompDef>(m_def);
            return def ? def->getSelfSimDelay() : TimeNs(0);
        }

        Json::Value toJson() const override {
            Json::Value json = SimComponent::toJson();
            return json;
        }

        static void fromJson(const std::shared_ptr<EvtBasedSimComponent> &comp,
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
    };

    class BESS_API EvtBasedSimDriver : public SimDriver {
      public:
        EvtBasedSimDriver() = default;
        ~EvtBasedSimDriver() override = default;

        void run() override {
            onBeforeRun();

            BESS_ASSERT(isInitialized(),
                        "SimDriver must be initialized before running");
            BESS_ASSERT(!isDestroyed(),
                        "SimDriver was destroyed, cannot run");

            while (!isStopped()) {
                std::unique_lock lk(m_runIterMutex);
                m_runIterCv.wait(lk, [&] {
                    return isStopped() || !getComponentsMap().empty();
                });

                if (isStopped()) {
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
        virtual bool simulate(const SimEvt &evt) = 0;

        virtual void addComponent(
            const std::shared_ptr<EvtBasedSimComponent> &comp,
            bool scheduleSim = true) {

            {
                std::lock_guard lk(m_compMapMutex);
                m_components[comp->getUuid()] = comp;
            }

            if (scheduleSim) {
                scheduleEvt(comp->getUuid(),
                            m_currentSimTime + comp->getSelfSimDelay(),
                            UUID::null,
                            true);
            }
        }

        virtual std::vector<UUID> getDependants(const UUID &id) {
            return {};
        }

        virtual std::vector<SlotState> collapseInputs(const UUID &id) {
            return {};
        }

        virtual void onBeforeRun() {}

        void onStop() override {
            SimDriver::onStop();
            m_runIterCv.notify_all();
        }

        void scheduleEvt(const UUID &compId,
                         TimeNs simTime,
                         const UUID &schedulerId,
                         bool notify = true) {
            static uint64_t evtId = 0;
            SimEvt ev{UUID(evtId++),
                      compId,
                      schedulerId,
                      simTime};
            m_events.insert(ev);
            if (notify) {
                m_runIterCv.notify_all();
            }
        }

      private:
        void simulateEvts(const std::vector<SimEvt> &evts) {
            using EvtComp = EvtBasedSimComponent;

            for (const auto &evt : evts) {
                const bool simDependants = simulate(evt);

                const auto &comp = getComponent<EvtComp>(evt.compId);

                if (!comp) {
                    BESS_WARN("(EvtBasedSimDriver.run) Component with UUID {} not found",
                              (uint64_t)evt.compId);
                    continue;
                }

                if (simDependants) {
                    scheduleDependantsOf(evt.compId);
                }

                if (comp->getSimSelf()) {
                    scheduleEvt(evt.compId,
                                m_currentSimTime + comp->getSelfSimDelay(),
                                UUID::null,
                                true);
                }
            }
        }

        void scheduleDependantsOf(const UUID &compId) {
            using EvtComp = EvtBasedSimComponent;

            const auto &comp = getComponent<EvtComp>(compId);

            if (!comp) {
                BESS_WARN("(EvtBasedSimDriver.scheduleDependantsOf) Component with UUID {} not found",
                          (uint64_t)compId);
                return;
            }

            const auto dependants = getDependants(compId);

            for (const auto &id : dependants) {
                scheduleEvt(id,
                            m_currentSimTime + comp->getPropDelay(),
                            compId,
                            false);
            }

            m_runIterCv.notify_all();
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

        std::vector<SimEvt> collectEvts() {
            std::set<UUID> collectedCompIds = {};
            std::vector<SimEvt> evtsToSim = {};

            for (const auto &evt : m_events) {
                if (evt.simTime > m_currentSimTime) {
                    continue;
                }

                if (collectedCompIds.contains(evt.compId)) {
                    continue;
                }

                collectedCompIds.insert(evt.compId);
                evtsToSim.push_back(evt);
            }

            // sort by sim time, then by event id to ensure deterministic order
            std::ranges::sort(evtsToSim,
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
        std::condition_variable m_runIterCv;
        std::mutex m_runIterMutex;

      private:
        std::set<SimEvt> m_events;
    };

} // namespace Bess::SimEngine::Drivers
