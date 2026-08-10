#include "sim_driver/sim_driver.h"
#include "common/bess_uuid.h"
#include "common/logger.h"
#include "common/types.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <utility>

namespace Bess::SimEngine::Drivers {
    namespace {
        bool sameWaveformValue(const PortState &lhs, const PortState &rhs) {
            if (lhs.signalKind != rhs.signalKind ||
                lhs.connState != rhs.connState) {
                return false;
            }

            const auto sameScalar = [](double a, double b) {
                return a == b || (std::isnan(a) && std::isnan(b));
            };

            switch (lhs.signalKind) {
            case SignalKind::scalar:
                return sameScalar(lhs.scalarValue, rhs.scalarValue);
            case SignalKind::vector:
                return lhs.vectorValue.size() == rhs.vectorValue.size() &&
                       std::ranges::equal(
                           lhs.vectorValue, rhs.vectorValue, sameScalar);
            case SignalKind::digital:
            case SignalKind::none:
                return lhs.getLogicState() == rhs.getLogicState();
            }
            return false;
        }

        bool sameWaveformState(const std::vector<PortState> &lhs,
                               const std::vector<PortState> &rhs) {
            return lhs.size() == rhs.size() &&
                   std::ranges::equal(lhs, rhs, sameWaveformValue);
        }
    } // namespace

    SimDriver::SimDriver()
        : m_simulationClock(std::make_shared<SimulationClock>()) {
    }

    Json::Value CompDef::toJson() const {
        Json::Value json;
        json["groupName"] = m_groupName;
        json["typeName"] = getTypeName();
        json["name"] = m_name;
        JsonConvert::toJsonValue(m_behaviorType, json["behaviorType"]);
        return json;
    }

    Json::Value SimComponent::toJson() const {
        Json::Value json;
        JsonConvert::toJsonValue(m_uuid, json["uuid"]);
        json["name"] = m_name;
        json["def"] = m_def ? m_def->toJson() : Json::Value();
        return json;
    }

    void SimComponent::loadJson(const Json::Value &json) {
        if (!json.isObject()) {
            return;
        }

        if (json.isMember("uuid")) {
            JsonConvert::fromJsonValue(json["uuid"], m_uuid);
        }

        if (json.isMember("name") && json["name"].isString()) {
            m_name = json["name"].asString();
        }
    }

    std::shared_ptr<SimFnDataBase>
    SimComponent::simulate(const std::shared_ptr<SimFnDataBase> &data) {
        if (!m_def) {
            BESS_WARN("(SimComponent.simulate) No definition for component "
                      "with UUID {}",
                      (uint64_t)m_uuid);
            return data;
        }

        auto simFn = m_def->getSimFn();
        if (simFn) {
            return simFn(data);
        }

        BESS_WARN("(SimComponent.simulate) No sim function for component "
                  "definition of component with UUID {}",
                  (uint64_t)m_uuid);
        return data;
    }

    bool SimDriver::hasComponent(const UUID &id) const {
        std::lock_guard lk(m_compMapMutex);
        return m_components.contains(id);
    }

    UUID SimDriver::addComponent(const std::shared_ptr<SimComponent> &comp,
                                 bool scheduleSim) {
        if (!comp) {
            return UUID::null;
        }

        {
            std::lock_guard lk(m_compMapMutex);
            m_components[comp->getUuid()] = comp;
        }

        onComponentAdded(comp);
        stampComponent(comp->getUuid(), getCurrentSimTime(), true);
        return comp->getUuid();
    }

    void SimDriver::deleteComponent(const UUID &uuid) {
        std::lock_guard lk(m_compMapMutex);
        m_components.erase(uuid);
    }

    void SimDriver::clearComponents() {
        std::lock_guard lk(m_compMapMutex);
        m_components.clear();
    }

    void SimDriver::publishStamps(
        std::vector<std::pair<UUID, ComponentStamp>> stamps,
        bool includeUnchanged) {
        // Publish the provided stamp batch atomically. Readers can never
        // observe only a prefix of a multi-component sample.
        std::vector<UUID> newlyTruncatedComponents;
        {
            std::lock_guard stampLock(m_stampMutex);
            for (auto &[componentId, stamp] : stamps) {
                auto &history = m_compStampData[componentId];

                const bool sameState =
                    !history.empty() &&
                    sameWaveformState(history.back().inputStates,
                                      stamp.inputStates) &&
                    sameWaveformState(history.back().outputStates,
                                      stamp.outputStates);
                if (!history.empty() &&
                    history.back().simTime == stamp.simTime && sameState) {
                    history.back() = std::move(stamp);
                    m_componentStampRevisions[componentId] =
                        ++m_nextStampRevision;
                    continue;
                }

                if (!includeUnchanged && sameState) {
                    continue;
                }

                if (history.size() >= MaxStampSamplesPerComponent) {
                    constexpr std::size_t retainedAfterTrim =
                        (MaxStampSamplesPerComponent * 3U) / 4U;
                    static_assert(retainedAfterTrim > 0);
                    history.erase(history.begin(),
                                  history.begin() +
                                      static_cast<std::ptrdiff_t>(
                                          history.size() - retainedAfterTrim));
                    if (m_truncatedStampComponents.insert(componentId).second) {
                        newlyTruncatedComponents.push_back(componentId);
                    }
                }

                history.emplace_back(std::move(stamp));
                m_componentStampRevisions[componentId] = ++m_nextStampRevision;
            }
        }

        for (const auto &componentId : newlyTruncatedComponents) {
            BESS_WARN("Stamp history for component {} in driver {} reached "
                      "the {}-sample retention limit; oldest samples will be "
                      "discarded",
                      static_cast<uint64_t>(componentId),
                      getName(),
                      MaxStampSamplesPerComponent);
        }
    }

    void SimDriver::stampComponent(const UUID &componentId,
                                   TimeNs simTime,
                                   bool includeUnchanged) {
        ComponentState state;
        try {
            state = getComponentState(componentId);
        } catch (const std::exception &error) {
            BESS_ERROR("Could not stamp component {} in driver {}: {}",
                       static_cast<uint64_t>(componentId),
                       getName(),
                       error.what());
            return;
        } catch (...) {
            BESS_ERROR("Could not stamp component {} in driver {}",
                       static_cast<uint64_t>(componentId),
                       getName());
            return;
        }

        std::vector<std::pair<UUID, ComponentStamp>> stamps;
        stamps.emplace_back(
            componentId,
            ComponentStamp{simTime, state.inputStates, state.outputStates});
        publishStamps(std::move(stamps), includeUnchanged);
    }

    void SimDriver::stampSim(TimeNs simTime, bool includeUnchanged) {
        std::vector<UUID> componentIds;
        {
            std::lock_guard lk(m_compMapMutex);
            componentIds.reserve(m_components.size());
            for (const auto &[componentId, _] : m_components) {
                componentIds.push_back(componentId);
            }
        }

        std::ranges::sort(componentIds, [](const UUID &a, const UUID &b) {
            return static_cast<uint64_t>(a) < static_cast<uint64_t>(b);
        });

        std::vector<std::pair<UUID, ComponentStamp>> stamps;
        stamps.reserve(componentIds.size());
        for (const auto &componentId : componentIds) {
            ComponentState state;
            try {
                state = getComponentState(componentId);
            } catch (const std::exception &error) {
                BESS_ERROR("Could not stamp component {} in driver {}: {}",
                           static_cast<uint64_t>(componentId),
                           getName(),
                           error.what());
                continue;
            } catch (...) {
                BESS_ERROR("Could not stamp component {} in driver {}",
                           static_cast<uint64_t>(componentId),
                           getName());
                continue;
            }
            stamps.emplace_back(
                componentId,
                ComponentStamp{simTime, state.inputStates, state.outputStates});
        }

        publishStamps(std::move(stamps), includeUnchanged);

        // Preserve the old extension point while keeping all storage and
        // sampling semantics centralized in the base driver.
        stampSim(std::chrono::duration_cast<TimeMs>(simTime));
    }

    SimDriver::StampDataView::StampDataView(const SimDriver &driver)
        : m_lock(driver.m_stampMutex),
          m_data(&driver.m_compStampData),
          m_revisions(&driver.m_componentStampRevisions),
          m_generation(driver.m_stampGeneration) {
    }

    const SimDriver::CompStampData &
    SimDriver::StampDataView::data() const noexcept {
        return *m_data;
    }

    std::optional<SimDriver::ComponentStampHistoryView>
    SimDriver::StampDataView::find(const UUID &componentId) const {
        const auto dataIt = m_data->find(componentId);
        if (dataIt == m_data->end()) {
            return std::nullopt;
        }

        const auto revisionIt = m_revisions->find(componentId);
        return ComponentStampHistoryView{
            .samples = dataIt->second,
            .generation = m_generation,
            .revision =
                revisionIt == m_revisions->end() ? 0 : revisionIt->second,
        };
    }

    SimDriver::StampDataView SimDriver::getStampData() const {
        return StampDataView(*this);
    }

    void SimDriver::clearStampData() {
        std::lock_guard lk(m_stampMutex);
        m_compStampData.clear();
        m_componentStampRevisions.clear();
        m_truncatedStampComponents.clear();
        ++m_stampGeneration;
    }

    bool SimDriver::isSimStable() const {
        return true;
    }

    ConnectionBundle SimDriver::getConnections(const UUID &uuid) const {
        return {};
    }

    std::vector<PortState> SimDriver::getInputPortStates(const UUID &compId) {
        return {};
    }

    PortState SimDriver::getPortState(const PortRef &port) const {
        return {LogicState::unknown, SimTime(0)};
    }

    bool SimDriver::setInputPortState(const UUID &uuid,
                                      int pinIdx,
                                      const PortState &state) {
        return false;
    }

    bool SimDriver::setOutputPortState(const UUID &uuid,
                                       int pinIdx,
                                       const PortState &state) {
        return false;
    }

    ComponentState SimDriver::getComponentState(const UUID &uuid) const {
        return {};
    }

    void SimDriver::propagateFromComponent(const UUID &sourceId) {
    }

    const std::unordered_map<UUID, Net> &SimDriver::getNetsMap() const {
        static const std::unordered_map<UUID, Net> empty;
        return empty;
    }

    bool SimDriver::isNetUpdated() const {
        return false;
    }

    void SimDriver::clearNetUpdated() {
    }

    void SimDriver::init() {
        onInit();
        std::lock_guard lk(m_stateMutex);
        m_state = SimDriverState::stopped;
    }

    bool SimDriver::beginRun(TimeNs startTime) {
        {
            std::lock_guard lk(m_stateMutex);
            if (m_state == SimDriverState::uninitialized ||
                m_state == SimDriverState::destroyed ||
                m_state == SimDriverState::running) {
                return false;
            }
            m_state = SimDriverState::running;
        }

        if (m_simulationClock) {
            m_simulationClock->reset(startTime);
        }

        clearStampData();
        try {
            std::vector<std::shared_ptr<SimComponent>> components;
            {
                std::lock_guard lk(m_compMapMutex);
                components.reserve(m_components.size());
                for (const auto &[_, component] : m_components) {
                    components.push_back(component);
                }
            }
            for (const auto &component : components) {
                if (component) {
                    component->resetRuntimeState(startTime);
                }
            }

            onRunStart(startTime);
            stampSim(startTime, true);
        } catch (...) {
            std::lock_guard lk(m_stateMutex);
            m_state = SimDriverState::stopped;
            throw;
        }
        return true;
    }

    bool SimDriver::isInitialized() const {
        std::lock_guard lk(m_stateMutex);
        return m_state != SimDriverState::uninitialized;
    }

    bool SimDriver::isRunning() const {
        std::lock_guard lk(m_stateMutex);
        return m_state == SimDriverState::running;
    }

    bool SimDriver::isPaused() const {
        std::lock_guard lk(m_stateMutex);
        return m_state == SimDriverState::paused;
    }

    bool SimDriver::isStopped() const {
        std::lock_guard lk(m_stateMutex);
        return m_state == SimDriverState::stopped;
    }

    bool SimDriver::isDestroyed() const {
        std::lock_guard lk(m_stateMutex);
        return m_state == SimDriverState::destroyed;
    }

    void SimDriver::pause() {
        bool changed = false;
        {
            std::lock_guard lk(m_stateMutex);
            if (m_state == SimDriverState::running) {
                m_state = SimDriverState::paused;
                changed = true;
            }
        }
        if (changed) {
            onPause();
        }
    }

    void SimDriver::resume() {
        bool changed = false;
        {
            std::lock_guard lk(m_stateMutex);
            if (m_state == SimDriverState::paused) {
                m_state = SimDriverState::running;
                changed = true;
            }
        }
        if (changed) {
            onResume();
        }
    }

    void SimDriver::stop() {
        bool changed = false;
        {
            std::lock_guard lk(m_stateMutex);
            if (m_state == SimDriverState::running ||
                m_state == SimDriverState::paused) {
                m_state = SimDriverState::stopped;
                changed = true;
            }
        }
        if (changed) {
            onStop();
        }
    }

    void SimDriver::reset() {
        onReset();
        std::lock_guard lk(m_stateMutex);
        m_state = SimDriverState::uninitialized;
    }

    void SimDriver::destroy() {
        if (isDestroyed()) {
            return;
        }

        m_onPortCountChangeCBs.clear();

        onDestroy();
        std::lock_guard lk(m_stateMutex);
        m_state = SimDriverState::destroyed;
    }

    void SimDriver::step() {
        if (isPaused()) {
            onStep();
        }
    }

    void SimDriver::addOnPortCountChangeCB(const UUID &id,
                                           const PortCountChangeCB &cb) {
        m_onPortCountChangeCBs[id] = cb;
    }

    void SimDriver::removeOnPortCountChangeCB(const UUID &id) {
        m_onPortCountChangeCBs.erase(id);
    }

    void SimDriver::triggerPortCountChangeCbs(const UUID &compId,
                                              PortDirection direction,
                                              SignalKind signalKind,
                                              int newCount) {
        for (const auto &[id, cb] : m_onPortCountChangeCBs) {
            cb(compId, direction, signalKind, newCount);
        }
    }

    void SimDriver::setSimulationClock(
        const std::shared_ptr<SimulationClock> &clock) {
        if (clock) {
            m_simulationClock = clock;
        }
    }

    std::shared_ptr<SimulationClock> SimDriver::getSimulationClock() const {
        return m_simulationClock;
    }

    TimeNs SimDriver::getCurrentSimTime() const {
        return m_simulationClock ? m_simulationClock->now() : TimeNs(0);
    }

    void SimDriver::setSchedulerNotifyFn(SchedulerNotifyFn notifyFn) {
        std::lock_guard lk(m_schedulerNotifyMutex);
        m_schedulerNotifyFn = std::move(notifyFn);
    }

    void SimDriver::notifyScheduler() {
        SchedulerNotifyFn notifyFn;
        {
            std::lock_guard lk(m_schedulerNotifyMutex);
            notifyFn = m_schedulerNotifyFn;
        }
        if (notifyFn) {
            notifyFn();
        }
    }

    void CompDef::loadJson(const Json::Value &json) {
        if (!json.isObject()) {
            return;
        }

        if (json.isMember("name") && json["name"].isString()) {
            m_name = json["name"].asString();
        }

        if (json.isMember("groupName") && json["groupName"].isString()) {
            m_groupName = json["groupName"].asString();
        }
    }

    PortDescriptor CompDef::getInputPortDescriptor() const {
        return {.direction = PortDirection::input};
    }

    PortDescriptor CompDef::getOutputPortDescriptor() const {
        return {.direction = PortDirection::output};
    }
} // namespace Bess::SimEngine::Drivers
