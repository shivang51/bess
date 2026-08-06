#include "simulation_engine.h"
#include "common/bess_uuid.h"
#include "common/logger.h"
#include "component_catalog.h"
// #include "dig_sim_driver.h"
#include "common/types.h"
#include "driver_registry.h"
#include "event_dispatcher.h"
#include "events/sim_engine_events.h"
#include "sim_driver/sim_driver.h"

#include "plugin_manager.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace Bess::SimEngine {
    namespace {
        thread_local SimulationEngine *g_activeSimulationEngine = nullptr;

        class ActiveSimulationThreadScope final {
          public:
            explicit ActiveSimulationThreadScope(SimulationEngine *engine)
                : m_previous(g_activeSimulationEngine) {
                g_activeSimulationEngine = engine;
            }

            ~ActiveSimulationThreadScope() {
                g_activeSimulationEngine = m_previous;
            }

          private:
            SimulationEngine *m_previous;
        };
    } // namespace

    void SimulationEngine::onInit() {
        loadDrivers();
        initDrivers();

        const auto &pluginMangaer = Plugins::PluginManager::getInstance();

        auto &catalog = ComponentCatalog::instance();
        for (const auto &plugin : pluginMangaer.getLoadedPlugins()) {
            const auto comps = plugin.second->onCompCatalogLoad();
            for (const auto &comp : comps) {
                catalog.registerComponent(comp);
            }
            BESS_INFO("Registered {} components from plugin {}",
                      comps.size(),
                      plugin.first);
        }
    }

    void SimulationEngine::onPostInit() {
        // run();
    }

    SimulationEngine::SimulationEngine()
        : m_simulationClock(std::make_shared<SimulationClock>()) {
    }

    SimRunCtx SimulationEngine::getRunCtx() const {
        std::lock_guard lk(m_stateMutex);
        return m_runCtx;
    }

    void SimulationEngine::setRunCtx(const SimRunCtx &runCtx) {
        std::lock_guard lk(m_stateMutex);
        m_runCtx = runCtx;
    }

    TimeNs SimulationEngine::getCurrentSimTime() const {
        return m_simulationClock ? m_simulationClock->now() : TimeNs(0);
    }

    Drivers::SimDriver::CompStampData SimulationEngine::getStampData() const {
        std::lock_guard executionLock(m_schedulerExecutionMutex);
        Drivers::SimDriver::CompStampData result;
        for (const auto &driver : m_simDrivers) {
            const auto driverData = driver->getStampData();
            for (const auto &[componentId, samples] : driverData) {
                result[componentId] = samples;
            }
        }
        return result;
    }

    void SimulationEngine::clearStampData() {
        std::lock_guard executionLock(m_schedulerExecutionMutex);
        for (const auto &driver : m_simDrivers) {
            driver->clearStampData();
        }
    }

    void SimulationEngine::clear(bool restoreState) {
        const auto previousState = getSimulationState();
        setSimulationState(SimulationState::stopped);
        clearPendingDriverEvents();

        for (auto &driver : m_simDrivers) {
            driver->clearComponents();
        }

        {
            std::lock_guard lkRegistry(m_pendingSignalSourcesMutex);
            m_pendingSignalSources.clear();
        }

        if (restoreState && previousState == SimulationState::running) {
            setSimulationState(SimulationState::running);
        }
    }

    SimulationEngine::~SimulationEngine() {
        destroy();
    }

    void SimulationEngine::onDestroy() {
        destroy();
    }

    void SimulationEngine::destroy() {
        if (m_destroyed)
            return;

        clear(false);

        destroyDrivers();
        unloadDrivers();

        m_stateCV.notify_all();

        ComponentCatalog::instance().destroy();

        m_destroyed = true;
    }

    const UUID &SimulationEngine::addComponent(
        const std::shared_ptr<Drivers::CompDef> &definition, bool cloneDef) {
        std::lock_guard executionLock(m_schedulerExecutionMutex);
        for (const auto &driver : m_simDrivers) {
            if (driver->supportsDef(definition)) {
                auto comp = driver->createComp(definition, cloneDef);
                if (!comp) {
                    return UUID::null;
                }

                driver->addComponent(comp, true);

                return comp->getUuid();
            }
        }

        BESS_WARN("No compatible driver found for {}.", definition->getName());
        return UUID::null;
    }

    std::pair<bool, std::string>
    SimulationEngine::canConnectPorts(const PortRef &src,
                                      const PortRef &dst) const {
        std::lock_guard executionLock(m_schedulerExecutionMutex);
        if (!src.isValid() || !dst.isValid()) {
            return {false, "Cannot connect to/from null component"};
        }

        std::shared_ptr<Drivers::SimDriver> srcDriver, dstDriver;
        for (const auto &driver : m_simDrivers) {
            if (driver->hasComponent(src.componentId))
                srcDriver = driver;
            if (driver->hasComponent(dst.componentId))
                dstDriver = driver;
        }

        if (!srcDriver || !dstDriver) {
            return {
                false,
                "Source or destination component does not exist in any driver"};
        }

        if (srcDriver != dstDriver) {
            return {false,
                    "Cross-driver connection is not currently supported "
                    "generically"};
        }

        return srcDriver->canConnectPorts(src, dst);
    }

    bool SimulationEngine::connectPorts(const PortRef &src,
                                        const PortRef &dst,
                                        bool overrideConn) {
        std::lock_guard executionLock(m_schedulerExecutionMutex);
        std::shared_ptr<Drivers::SimDriver> srcDriver, dstDriver;
        for (const auto &driver : m_simDrivers) {
            if (driver->hasComponent(src.componentId))
                srcDriver = driver;
            if (driver->hasComponent(dst.componentId))
                dstDriver = driver;
        }

        if (!srcDriver || !dstDriver || srcDriver != dstDriver) {
            return false;
        }

        std::lock_guard lk(m_driversMutex);
        return srcDriver->connectPorts(src, dst, overrideConn);
    }

    void SimulationEngine::deleteComponent(const UUID &uuid) {
        std::lock_guard executionLock(m_schedulerExecutionMutex);
        if (uuid == UUID::null || !getComponentDefinition(uuid)) {
            return;
        }

        std::lock_guard lk(m_driversMutex);

        for (const auto &driver : m_simDrivers) {
            if (driver->hasComponent(uuid)) {
                driver->deleteComponent(uuid);
                break;
            }
        }

        {
            std::lock_guard pendingLock(m_pendingSignalSourcesMutex);
            m_pendingSignalSources.erase(uuid);
        }

        BESS_INFO("Deleted component {}", (uint64_t)uuid);
    }

    PortState SimulationEngine::getPortState(const PortRef &port) {
        std::lock_guard executionLock(m_schedulerExecutionMutex);
        if (!port.isValid() || !getComponentDefinition(port.componentId)) {
            BESS_WARN("[getDigitalPinState] Component with UUID {} is invalid",
                      (uint64_t)port.componentId);
            return {LogicState::low, SimTime(0)};
        }

        for (const auto &driver : m_simDrivers) {
            if (driver->hasComponent(port.componentId)) {
                return driver->getPortState(port);
            }
        }

        return {LogicState::unknown, SimTime(0)};
    }

    ConnectionBundle SimulationEngine::getConnections(const UUID &uuid) {
        std::lock_guard executionLock(m_schedulerExecutionMutex);
        ConnectionBundle bundle;

        if (!getComponentDefinition(uuid)) {
            BESS_WARN("[getConnections] Component with UUID {} is invalid",
                      (uint64_t)uuid);
            return bundle;
        }

        for (const auto &driver : m_simDrivers) {
            if (driver->hasComponent(uuid)) {
                return driver->getConnections(uuid);
            }
        }

        return bundle;
    }

    void SimulationEngine::setInputPortState(const UUID &uuid,
                                             int pinIdx,
                                             const PortState &state) {
        std::lock_guard executionLock(m_schedulerExecutionMutex);
        for (const auto &driver : m_simDrivers) {
            if (driver->hasComponent(uuid)) {
                driver->setInputPortState(uuid, pinIdx, state);
                return;
            }
        }

        BESS_WARN("[setInputPortState] Component with UUID {} is invalid",
                  (uint64_t)uuid);
    }

    void SimulationEngine::setOutputPortState(const UUID &uuid,
                                              int pinIdx,
                                              const PortState &state) {
        std::lock_guard executionLock(m_schedulerExecutionMutex);
        for (const auto &driver : m_simDrivers) {
            if (driver->hasComponent(uuid)) {
                driver->setOutputPortState(uuid, pinIdx, state);
                return;
            }
        }

        BESS_WARN("[setOutputPortState] Component with UUID {} is invalid",
                  (uint64_t)uuid);
    }

    void SimulationEngine::setInputPortState(const UUID &uuid,
                                             int pinIdx,
                                             LogicState state) {
        setInputPortState(uuid, pinIdx, PortState::digital(state));
    }

    const ComponentState &
    SimulationEngine::getComponentState(const UUID &uuid) {
        std::lock_guard executionLock(m_schedulerExecutionMutex);
        static thread_local ComponentState snapshot;
        snapshot = {};

        for (const auto &driver : m_simDrivers) {
            if (driver->hasComponent(uuid)) {
                snapshot = driver->getComponentState(uuid);
                return snapshot;
            }
        }

        return snapshot;
    }

    const std::shared_ptr<Drivers::CompDef> &
    SimulationEngine::getComponentDefinition(const UUID &uuid) const {
        static const std::shared_ptr<Drivers::CompDef> nullDef;
        const auto comp = getComponent<Drivers::SimComponent>(uuid);
        if (!comp) {
            return nullDef;
        }
        return comp->getDefinition();
    }

    void SimulationEngine::deleteConnection(const PortRef &portA,
                                            const PortRef &portB) {
        std::lock_guard executionLock(m_schedulerExecutionMutex);
        std::shared_ptr<Drivers::SimDriver> aDriver, bDriver;
        for (const auto &driver : m_simDrivers) {
            if (driver->hasComponent(portA.componentId))
                aDriver = driver;
            if (driver->hasComponent(portB.componentId))
                bDriver = driver;
        }

        if (!aDriver || !bDriver || aDriver != bDriver) {
            return;
        }

        std::lock_guard lk(m_driversMutex);
        aDriver->deleteConnection(portA, portB);
    }

    std::vector<PortState>
    SimulationEngine::getInputPortStates(UUID compId) const {
        std::lock_guard executionLock(m_schedulerExecutionMutex);
        for (const auto &driver : m_simDrivers) {
            if (driver->hasComponent(compId)) {
                return driver->getInputPortStates(compId);
            }
        }

        return {};
    }

    SimulationState SimulationEngine::getSimulationState() const {
        return m_simState.load();
    }

    void SimulationEngine::stepSimulation() {
        if (m_simState.load() != SimulationState::paused ||
            m_stepFlag.exchange(true)) {
            return;
        }

        std::lock_guard lifecycleLock(m_lifecycleMutex);
        std::lock_guard executionLock(m_schedulerExecutionMutex);
        if (m_simState.load() != SimulationState::paused) {
            m_stepFlag.store(false);
            return;
        }

        ActiveSimulationThreadScope activeScope(this);
        const auto action = getNextSchedulerAction();
        if (action) {
            const bool converged = executeSchedulerAction(*action);
            if (!converged || action->final) {
                completeRun(m_runGeneration.load());
            }
        }

        m_stepFlag.store(false);
    }

    void SimulationEngine::setOutputPortState(const UUID &uuid,
                                              int pinIdx,
                                              LogicState state) {
        setOutputPortState(uuid, pinIdx, PortState::digital(state));
    }

    SimulationState SimulationEngine::toggleStartStop() {
        if (m_simState == SimulationState::stopped) {
            setSimulationState(SimulationState::running);
        } else if (m_simState == SimulationState::running ||
                   m_simState == SimulationState::paused) {
            setSimulationState(SimulationState::stopped);
        }

        return m_simState.load();
    }

    SimulationState SimulationEngine::togglePlayPause() {
        if (m_simState == SimulationState::running) {
            setSimulationState(SimulationState::paused);
        } else if (m_simState == SimulationState::paused) {
            setSimulationState(SimulationState::running);
        }

        return m_simState.load();
    }

    void SimulationEngine::setSimulationState(SimulationState state) {
        if (state == SimulationState::running) {
            if (m_simState.load() == SimulationState::paused) {
                std::unique_lock lifecycleLock(m_lifecycleMutex,
                                               std::defer_lock);
                if (g_activeSimulationEngine != this) {
                    lifecycleLock.lock();
                }

                std::lock_guard executionLock(m_schedulerExecutionMutex);
                if (m_simState.load() != SimulationState::paused) {
                    return;
                }
                {
                    std::lock_guard stateLock(m_stateMutex);
                    m_simState.store(SimulationState::running);
                    m_runCtx.simState = SimulationState::running;
                    ++m_schedulerRevision;
                }

                resumeDrivers();
                if (m_simState.load() == SimulationState::running) {
                    processPendingPropagation();
                }
                m_stateCV.notify_all();
            } else if (m_simState.load() == SimulationState::stopped) {
                run();
            }
        } else if (state == SimulationState::stopped) {
            stop();
        } else if (state == SimulationState::paused) {
            std::unique_lock lifecycleLock(m_lifecycleMutex, std::defer_lock);
            if (g_activeSimulationEngine != this) {
                lifecycleLock.lock();
            }
            if (m_simState.load() != SimulationState::running) {
                return;
            }
            {
                std::lock_guard stateLock(m_stateMutex);
                m_simState.store(SimulationState::paused);
                m_runCtx.simState = SimulationState::paused;
                ++m_schedulerRevision;
            }
            m_stateCV.notify_all();

            // A pause is a synchronization barrier: when this call returns,
            // an already-running event batch has finished and no new virtual
            // time can be consumed until resume.
            std::lock_guard executionLock(m_schedulerExecutionMutex);
            if (m_simState.load() == SimulationState::stopped) {
                return;
            }
            if (m_simState.load() != SimulationState::paused) {
                std::lock_guard stateLock(m_stateMutex);
                m_simState.store(SimulationState::paused);
                m_runCtx.simState = SimulationState::paused;
                ++m_schedulerRevision;
            }
            pauseDrivers();
            m_stateCV.notify_all();
        }
    }

    void SimulationEngine::clearPendingDriverEvents() {
        std::lock_guard executionLock(m_schedulerExecutionMutex);
        for (auto &driver : m_simDrivers) {
            driver->clearPendingEvents();
        }
    }

    void SimulationEngine::run() {
        startRun(false, TimeNs(0), TimeNs(0));
    }

    void SimulationEngine::runFor(TimeMs duration, TimeMs stepInterval) {
        if (!std::isfinite(duration.count()) || duration.count() < 0.0) {
            BESS_WARN("Invalid timed-run duration of {}ms; using 0ms",
                      duration.count());
            duration = TimeMs(0);
        }
        if (!std::isfinite(stepInterval.count()) ||
            stepInterval.count() < 0.0) {
            BESS_WARN("Invalid timed-run sample interval of {}ms; sampling "
                      "only the initial and final state",
                      stepInterval.count());
            stepInterval = TimeMs(0);
        }

        auto durationNs = std::chrono::duration_cast<TimeNs>(duration);
        if (!std::isfinite(durationNs.count())) {
            BESS_WARN("Timed-run duration exceeds the simulation clock range; "
                      "it was clamped");
            durationNs = TimeNs::max();
        }

        auto sampleIntervalNs =
            std::chrono::duration_cast<TimeNs>(stepInterval);
        if (!std::isfinite(sampleIntervalNs.count())) {
            BESS_WARN("Timed-run sample interval exceeds the simulation clock "
                      "range; it was clamped");
            sampleIntervalNs = TimeNs::max();
        }

        startRun(true, durationNs, sampleIntervalNs);
    }

    void SimulationEngine::stop() {
        // A callback running on a simulation-owned thread cannot join itself.
        // Request the stop here; the next controlling-thread lifecycle call
        // will collect the finished thread.
        if (g_activeSimulationEngine == this) {
            requestStop();
            return;
        }

        std::lock_guard lifecycleLock(m_lifecycleMutex);
        stopLocked();
    }

    void SimulationEngine::requestStop() {
        {
            std::lock_guard stateLock(m_stateMutex);
            m_simState.store(SimulationState::stopped);
            m_runCtx.simState = SimulationState::stopped;
            ++m_schedulerRevision;
            ++m_runGeneration;
        }

        // Wake a paced scheduler before waiting for its current action to
        // leave the execution barrier.
        m_stateCV.notify_all();
        std::lock_guard executionLock(m_schedulerExecutionMutex);
        requestDriverStops();
    }

    void SimulationEngine::stopLocked() {
        requestStop();

        if (m_schedulerThread.joinable() &&
            m_schedulerThread.get_id() != std::this_thread::get_id()) {
            m_schedulerThread.join();
        }
        stopDrivers();
    }

    void SimulationEngine::startRun(bool timedRun,
                                    TimeNs duration,
                                    TimeNs sampleInterval) {
        if (g_activeSimulationEngine == this) {
            BESS_ERROR("A simulation run cannot be restarted from a driver "
                       "or scheduler callback; request the restart from the "
                       "controlling thread");
            return;
        }

        std::lock_guard lifecycleLock(m_lifecycleMutex);

        if (m_schedulerThread.joinable() &&
            m_schedulerThread.get_id() == std::this_thread::get_id()) {
            BESS_ERROR("A simulation run cannot be restarted from its own "
                       "scheduler callback; request the restart from the "
                       "controlling thread");
            return;
        }

        // Keep stop + reconfiguration + thread creation in one lifecycle
        // transaction. Concurrent run()/runFor() calls can never overwrite a
        // joinable scheduler thread or reset the clock underneath a new run.
        stopLocked();
        m_simulationClock->reset(TimeNs(0));
        clearStampData();

        {
            std::lock_guard stateLock(m_stateMutex);
            m_runEndTime = timedRun ? duration : TimeNs::max();
            m_sampleInterval = sampleInterval;
            m_nextSampleTime = TimeNs(0);
            m_initialSamplePending = true;

            m_runCtx.runDuration =
                timedRun ? std::chrono::duration_cast<TimeMs>(duration)
                         : TimeMs(0);
            m_runCtx.stepInterval =
                timedRun ? std::chrono::duration_cast<TimeMs>(sampleInterval)
                         : TimeMs(0);
            m_runCtx.elapsedTime = TimeMs(0);
            m_runCtx.isTimedRun = timedRun;
            m_runCtx.simState = SimulationState::running;
            m_simState.store(SimulationState::running);
            ++m_runGeneration;
            ++m_schedulerRevision;
        }

        const bool driversStarted = runDrivers();
        if (!driversStarted || m_simState.load() != SimulationState::running) {
            if (!driversStarted) {
                BESS_ERROR("Simulation run was not started because one or "
                           "more drivers failed to start");
            }
            {
                std::lock_guard stateLock(m_stateMutex);
                m_simState.store(SimulationState::stopped);
                m_runCtx.simState = SimulationState::stopped;
                ++m_runGeneration;
                ++m_schedulerRevision;
            }
            stopDrivers();
            m_stateCV.notify_all();
            return;
        }

        const auto generation = m_runGeneration.load();
        try {
            m_schedulerThread = std::thread([this, generation]() {
                schedulerLoop(generation, true);
            });
        } catch (const std::exception &error) {
            BESS_ERROR("Failed to create the simulation scheduler thread: {}",
                       error.what());
            {
                std::lock_guard stateLock(m_stateMutex);
                m_simState.store(SimulationState::stopped);
                m_runCtx.simState = SimulationState::stopped;
                ++m_runGeneration;
                ++m_schedulerRevision;
            }
            stopDrivers();
        }
        m_stateCV.notify_all();
    }

    std::optional<TimeNs> SimulationEngine::getNextGlobalEventTime() {
        std::optional<TimeNs> result;
        for (const auto &driver : m_simDrivers) {
            if (!driver->usesGlobalClockScheduling()) {
                continue;
            }

            try {
                const auto next = driver->getNextEventTime();
                if (!next) {
                    continue;
                }
                if (!std::isfinite(next->count()) || next->count() < 0.0) {
                    BESS_ERROR("Driver {} returned an invalid next-event "
                               "time of {}ns; stopping the run",
                               driver->getName(),
                               next->count());
                    requestStop();
                    return std::nullopt;
                }
                if (!result || *next < *result) {
                    result = *next;
                }
            } catch (const std::exception &error) {
                BESS_ERROR("Driver {} failed while selecting its next event: "
                           "{}",
                           driver->getName(),
                           error.what());
                requestStop();
                return std::nullopt;
            } catch (...) {
                BESS_ERROR("Driver {} failed while selecting its next event",
                           driver->getName());
                requestStop();
                return std::nullopt;
            }
        }
        return result;
    }

    std::optional<SimulationEngine::SchedulerAction>
    SimulationEngine::getNextSchedulerAction() {
        const auto currentTime = getCurrentSimTime();
        const auto runCtx = getRunCtx();
        std::optional<TimeNs> target = getNextGlobalEventTime();
        if (m_simState.load() == SimulationState::stopped) {
            return std::nullopt;
        }

        if (m_initialSamplePending && (!target || currentTime < *target)) {
            target = currentTime;
        }

        if (runCtx.isTimedRun) {
            if (!target || m_nextSampleTime < *target) {
                target = m_nextSampleTime;
            }
            if (!target || m_runEndTime < *target) {
                target = m_runEndTime;
            }
        }

        if (!target) {
            return std::nullopt;
        }

        if (*target < currentTime) {
            *target = currentTime;
        }

        const bool sample = m_initialSamplePending ||
                            (runCtx.isTimedRun && m_nextSampleTime <= *target);
        const bool final = runCtx.isTimedRun && *target >= m_runEndTime;
        return SchedulerAction{*target, sample, final};
    }

    void SimulationEngine::schedulerLoop(uint64_t generation,
                                         bool realTimePaced) {
        ActiveSimulationThreadScope activeScope(this);
        using Clock = std::chrono::steady_clock;
        auto realTimeOrigin = Clock::now();

        const auto realTimeDeadline = [&](TimeNs simTime) {
            using WideNs = std::chrono::duration<long double, std::nano>;
            const auto maxDelay = Clock::time_point::max() - realTimeOrigin;
            const auto maxDelayNs =
                std::chrono::duration_cast<WideNs>(maxDelay);
            const auto requestedNs =
                WideNs(static_cast<long double>(simTime.count()));
            if (requestedNs >= maxDelayNs) {
                return Clock::time_point::max();
            }
            return realTimeOrigin +
                   std::chrono::duration_cast<Clock::duration>(requestedNs);
        };

        try {
            while (true) {
                uint64_t observedRevision = 0;
                {
                    std::unique_lock stateLock(m_stateMutex);
                    bool resumedFromPause = false;
                    while (m_simState.load() == SimulationState::paused &&
                           generation == m_runGeneration.load()) {
                        resumedFromPause = true;
                        m_stateCV.wait(stateLock);
                    }

                    if (m_simState.load() != SimulationState::running ||
                        generation != m_runGeneration.load()) {
                        return;
                    }

                    if (resumedFromPause && realTimePaced) {
                        realTimeOrigin =
                            Clock::now() -
                            std::chrono::duration_cast<Clock::duration>(
                                getCurrentSimTime());
                    }
                    observedRevision = m_schedulerRevision;
                }

                auto action = getNextSchedulerAction();
                if (!action) {
                    std::unique_lock stateLock(m_stateMutex);
                    m_stateCV.wait(stateLock, [&] {
                        return m_simState.load() != SimulationState::running ||
                               generation != m_runGeneration.load() ||
                               m_schedulerRevision != observedRevision;
                    });
                    continue;
                }

                if (realTimePaced && action->time > getCurrentSimTime()) {
                    const auto deadline = realTimeDeadline(action->time);
                    std::unique_lock stateLock(m_stateMutex);
                    const bool interrupted =
                        m_stateCV.wait_until(stateLock, deadline, [&] {
                            return m_simState.load() !=
                                       SimulationState::running ||
                                   generation != m_runGeneration.load() ||
                                   m_schedulerRevision != observedRevision;
                        });
                    if (interrupted) {
                        continue;
                    }
                }

                std::lock_guard executionLock(m_schedulerExecutionMutex);
                if (m_simState.load() != SimulationState::running ||
                    generation != m_runGeneration.load()) {
                    continue;
                }

                // An event may have been inserted between the wait and this
                // lock. Re-selecting guarantees global timestamp order.
                const auto selected = getNextSchedulerAction();
                if (!selected || selected->time != action->time) {
                    continue;
                }
                action = selected;

                const bool converged = executeSchedulerAction(*action);

                if (!converged || action->final) {
                    completeRun(generation);
                    return;
                }
            }
        } catch (const std::exception &error) {
            BESS_ERROR("Simulation scheduler stopped after an exception: {}",
                       error.what());
        } catch (...) {
            BESS_ERROR("Simulation scheduler stopped after an unknown "
                       "exception");
        }

        completeRun(generation);
    }

    bool
    SimulationEngine::executeSchedulerAction(const SchedulerAction &action) {
        const auto nextEvent = getNextGlobalEventTime();
        const bool hasEventAtAction = nextEvent && *nextEvent <= action.time;

        if (!m_simulationClock->advanceTo(action.time)) {
            BESS_ERROR("Could not advance the global simulation clock to "
                       "{}ns",
                       action.time.count());
            return false;
        }

        const bool converged = settleDriversAt(action.time);
        updateElapsedTime(action.time);

        if (action.sample) {
            stampSim(action.time, true);
            m_initialSamplePending = false;
            advanceSampleTime(action.time);
        } else if (!getRunCtx().isTimedRun && hasEventAtAction) {
            stampSim(action.time, false);
        }

        if (action.final && !action.sample) {
            stampSim(action.time, true);
        }
        return converged;
    }

    bool SimulationEngine::settleDriversAt(TimeNs simTime) {
        constexpr size_t maxDeltaCycles = 100000;

        for (size_t deltaCycle = 0; deltaCycle < maxDeltaCycles; ++deltaCycle) {
            bool processedAny = false;
            for (const auto &driver : m_simDrivers) {
                if (!driver->usesGlobalClockScheduling()) {
                    continue;
                }

                try {
                    const auto next = driver->getNextEventTime();
                    if (next && *next <= simTime) {
                        const auto processed = driver->processEventsAt(simTime);
                        if (processed == 0) {
                            BESS_ERROR("Driver {} reported a due event at "
                                       "{}ns but made no progress",
                                       driver->getName(),
                                       next->count());
                            return false;
                        }
                        processedAny = true;
                    }
                } catch (const std::exception &error) {
                    BESS_ERROR("Driver {} failed while processing events at "
                               "{}ns: {}",
                               driver->getName(),
                               simTime.count(),
                               error.what());
                    return false;
                } catch (...) {
                    BESS_ERROR("Driver {} failed while processing events at "
                               "{}ns",
                               driver->getName(),
                               simTime.count());
                    return false;
                }
            }

            if (!processedAny) {
                return true;
            }
        }

        const auto remainingEvent = getNextGlobalEventTime();
        if (!remainingEvent || *remainingEvent > simTime) {
            return true;
        }

        BESS_ERROR("Simulation failed to converge at {}ns after {} delta "
                   "cycles; stopping the run",
                   simTime.count(),
                   maxDeltaCycles);
        return false;
    }

    void SimulationEngine::completeRun(uint64_t generation) {
        {
            std::lock_guard stateLock(m_stateMutex);
            if (generation != m_runGeneration.load()) {
                return;
            }
            m_simState.store(SimulationState::stopped);
            m_runCtx.simState = SimulationState::stopped;
            ++m_schedulerRevision;
        }

        // Only request driver shutdown here. Joining legacy driver threads
        // while the scheduler owns the execution barrier can deadlock a
        // driver that is finishing an engine call. The controlling thread
        // collects finished threads in stopLocked() before the next run or
        // destruction.
        requestDriverStops();
        m_stateCV.notify_all();
    }

    void SimulationEngine::notifyScheduler() {
        {
            std::lock_guard stateLock(m_stateMutex);
            ++m_schedulerRevision;
        }
        m_stateCV.notify_all();
    }

    void SimulationEngine::updateElapsedTime(TimeNs simTime) {
        std::lock_guard stateLock(m_stateMutex);
        m_runCtx.elapsedTime = std::chrono::duration_cast<TimeMs>(simTime);
    }

    void SimulationEngine::advanceSampleTime(TimeNs processedTime) {
        if (m_sampleInterval.count() <= 0.0) {
            m_nextSampleTime = TimeNs::max();
            return;
        }

        do {
            const auto previous = m_nextSampleTime;
            m_nextSampleTime += m_sampleInterval;
            if (!std::isfinite(m_nextSampleTime.count()) ||
                m_nextSampleTime <= previous) {
                m_nextSampleTime = TimeNs::max();
                return;
            }
        } while (m_nextSampleTime <= processedTime);
    }

    bool SimulationEngine::isNetUpdated() const {
        std::lock_guard executionLock(m_schedulerExecutionMutex);
        for (const auto &driver : m_simDrivers) {
            if (driver->isNetUpdated()) {
                return true;
            }
        }

        return false;
    }

    std::unordered_map<UUID, Net> SimulationEngine::getNetsMap(bool update) {
        std::lock_guard executionLock(m_schedulerExecutionMutex);
        std::unordered_map<UUID, Net> nets;
        for (const auto &driver : m_simDrivers) {
            const auto &driverNets = driver->getNetsMap();
            nets.insert(driverNets.begin(), driverNets.end());
        }

        if (update) {
            for (const auto &driver : m_simDrivers) {
                driver->clearNetUpdated();
            }
        }

        return nets;
    }

    void SimulationEngine::triggerPropagation(const UUID &sourceId) {
        std::lock_guard executionLock(m_schedulerExecutionMutex);
        for (const auto &driver : m_simDrivers) {
            if (driver->hasComponent(sourceId)) {
                driver->propagateFromComponent(sourceId);
                return;
            }
        }
    }

    void SimulationEngine::markPendingSignalSource(const UUID &sourceId) {
        std::lock_guard lk(m_pendingSignalSourcesMutex);
        m_pendingSignalSources.insert(sourceId);
    }

    bool SimulationEngine::addPort(const PortRef &port, bool force) {
        std::lock_guard executionLock(m_schedulerExecutionMutex);
        for (const auto &driver : m_simDrivers) {
            if (driver->hasComponent(port.componentId)) {
                const auto res = driver->addPort(port, force);

                if (!res.hasChange())
                    return false;

                if (res.changedInputs) {
                    Events::CompDefInputsResizedEvent event{port.componentId};
                    auto &appCtx = GAppContext::getInstance();
                    auto eventDispatcher =
                        appCtx
                            .getSubSystem<Bess::EventSystem::EventDispatcher>();
                    eventDispatcher->queue(event);
                }

                if (res.changedOutputs) {
                    Events::CompDefOutputsResizedEvent event{port.componentId};
                    auto &appCtx = GAppContext::getInstance();
                    auto eventDispatcher =
                        appCtx
                            .getSubSystem<Bess::EventSystem::EventDispatcher>();
                    eventDispatcher->queue(event);
                }

                return true;
            }
        }
        return false;
    }

    bool SimulationEngine::removePort(const PortRef &port, bool force) {
        std::lock_guard executionLock(m_schedulerExecutionMutex);
        for (const auto &driver : m_simDrivers) {
            if (driver->hasComponent(port.componentId)) {
                const auto res = driver->removePort(port, force);

                if (!res.hasChange())
                    return false;

                if (res.changedInputs) {
                    Events::CompDefInputsResizedEvent event{port.componentId};
                    auto &appCtx = GAppContext::getInstance();
                    auto eventDispatcher =
                        appCtx
                            .getSubSystem<Bess::EventSystem::EventDispatcher>();
                    eventDispatcher->queue(event);
                }

                if (res.changedOutputs) {
                    Events::CompDefOutputsResizedEvent event{port.componentId};
                    auto &appCtx = GAppContext::getInstance();
                    auto eventDispatcher =
                        appCtx
                            .getSubSystem<Bess::EventSystem::EventDispatcher>();
                    eventDispatcher->queue(event);
                }

                return true;
            }
        }
        return false;
    }

    bool SimulationEngine::isSimStable() {
        std::lock_guard executionLock(m_schedulerExecutionMutex);
        for (const auto &driver : m_simDrivers) {
            if (!driver->isSimStable()) {
                return false;
            }
        }

        return true;
    }

    const std::vector<std::shared_ptr<Drivers::SimDriver>> &
    SimulationEngine::getDrivers() const {
        return m_simDrivers;
    }

    void SimulationEngine::propagateFromComponent(const UUID &sourceId) {
        triggerPropagation(sourceId);
    }

    void SimulationEngine::processPendingPropagation() {
        std::set<UUID> pending;
        {
            std::lock_guard lk(m_pendingSignalSourcesMutex);
            pending.swap(m_pendingSignalSources);
        }

        for (const auto &sourceId : pending) {
            triggerPropagation(sourceId);
        }
    }

    void SimulationEngine::loadDrivers() {
        for (const auto &[name, loader] : DriverRegistry::getRegistry()) {
            auto driver = loader();
            if (driver) {
                driver->setEngine(this);
                driver->setSimulationClock(m_simulationClock);
                driver->setSchedulerNotifyFn([this]() { notifyScheduler(); });
                m_simDrivers.push_back(driver);
                BESS_INFO("Loaded driver {}", driver->getName());
            } else {
                BESS_ERROR("Failed to load driver {}", name);
            }
        }

        std::ranges::sort(m_simDrivers, [](const auto &a, const auto &b) {
            return a->getName() < b->getName();
        });
    }

    void SimulationEngine::unloadDrivers() {
        const size_t n = m_simDrivers.size();
        for (const auto &driver : m_simDrivers) {
            driver->setSchedulerNotifyFn({});
            driver->setEngine(nullptr);
        }
        m_simDrivers.clear();
        BESS_INFO("Unloaded all({}) drivers", n);
    }

    void SimulationEngine::initDrivers() {
        for (auto &driver : m_simDrivers) {
            driver->init();
            BESS_INFO("Initialized driver {}", driver->getName());
        }
    }

    void SimulationEngine::destroyDrivers() {
        for (auto &driver : m_simDrivers) {
            driver->destroy();
            BESS_INFO("Destroyed driver {}", driver->getName());
        }

        std::lock_guard threadLock(m_driverThreadMutex);
        m_legacyDriverThreads.clear();
    }

    bool SimulationEngine::runDrivers() {
        ActiveSimulationThreadScope activeScope(this);
        bool allStarted = true;
        for (auto &driver : m_simDrivers) {
            if (driver->usesGlobalClockScheduling()) {
                bool started = false;
                try {
                    started = driver->beginRun(TimeNs(0));
                } catch (const std::exception &error) {
                    BESS_ERROR("Coordinated driver {} threw while starting: "
                               "{}",
                               driver->getName(),
                               error.what());
                } catch (...) {
                    BESS_ERROR("Coordinated driver {} threw while starting",
                               driver->getName());
                }

                if (!started) {
                    BESS_ERROR("Failed to start coordinated driver {}",
                               driver->getName());
                    allStarted = false;
                } else {
                    BESS_INFO("Started coordinated driver {}",
                              driver->getName());
                }
                continue;
            }

            std::lock_guard threadLock(m_driverThreadMutex);
            try {
                m_legacyDriverThreads.emplace_back([this, driver]() {
                    ActiveSimulationThreadScope activeScope(this);
                    try {
                        driver->run();
                    } catch (const std::exception &error) {
                        BESS_ERROR("Legacy driver {} stopped after an "
                                   "exception: {}",
                                   driver->getName(),
                                   error.what());
                        requestStop();
                    } catch (...) {
                        BESS_ERROR("Legacy driver {} stopped after an unknown "
                                   "exception",
                                   driver->getName());
                        requestStop();
                    }
                });
            } catch (const std::exception &error) {
                BESS_ERROR("Failed to create the run thread for legacy driver "
                           "{}: {}",
                           driver->getName(),
                           error.what());
                allStarted = false;
                continue;
            }
            BESS_WARN("Driver {} uses the legacy independent run loop; it "
                      "shares the global clock but cannot participate in "
                      "deterministic event barriers",
                      driver->getName());
        }
        return allStarted;
    }

    void SimulationEngine::requestDriverStops() {
        ActiveSimulationThreadScope activeScope(this);
        for (auto &driver : m_simDrivers) {
            const bool wasActive = driver->isRunning() || driver->isPaused();
            try {
                driver->stop();
                if (wasActive) {
                    BESS_INFO("Stopped driver {}", driver->getName());
                }
            } catch (const std::exception &error) {
                BESS_ERROR("Driver {} threw while stopping: {}",
                           driver->getName(),
                           error.what());
            } catch (...) {
                BESS_ERROR("Driver {} threw while stopping", driver->getName());
            }
        }
    }

    void SimulationEngine::stopDrivers() {
        requestDriverStops();

        std::lock_guard threadLock(m_driverThreadMutex);
        for (auto &thread : m_legacyDriverThreads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        m_legacyDriverThreads.clear();
    }

    void SimulationEngine::pauseDrivers() {
        ActiveSimulationThreadScope activeScope(this);
        for (auto &driver : m_simDrivers) {
            try {
                driver->pause();
                BESS_INFO("Paused driver {}", driver->getName());
            } catch (const std::exception &error) {
                BESS_ERROR("Driver {} threw while pausing: {}",
                           driver->getName(),
                           error.what());
            } catch (...) {
                BESS_ERROR("Driver {} threw while pausing", driver->getName());
            }
        }
    }

    void SimulationEngine::resumeDrivers() {
        ActiveSimulationThreadScope activeScope(this);
        for (auto &driver : m_simDrivers) {
            try {
                driver->resume();
                BESS_INFO("Resumed driver {}", driver->getName());
            } catch (const std::exception &error) {
                BESS_ERROR("Driver {} threw while resuming: {}",
                           driver->getName(),
                           error.what());
            } catch (...) {
                BESS_ERROR("Driver {} threw while resuming", driver->getName());
            }
        }
    }

    void SimulationEngine::stampSim(TimeNs simTime, bool includeUnchanged) {
        for (const auto &driver : m_simDrivers) {
            try {
                driver->stampSim(simTime, includeUnchanged);
            } catch (const std::exception &error) {
                BESS_ERROR("Driver {} failed while stamping at {}ns: {}",
                           driver->getName(),
                           simTime.count(),
                           error.what());
            } catch (...) {
                BESS_ERROR("Driver {} failed while stamping at {}ns",
                           driver->getName(),
                           simTime.count());
            }
        }
    }

    void SimulationEngine::addOnPortCountChangeCB(
        const UUID &id, const Drivers::PortCountChangeCB &cb) {
        for (const auto &driver : m_simDrivers) {
            if (driver->hasComponent(id)) {
                driver->addOnPortCountChangeCB(id, cb);
                BESS_DEBUG("Added port count change callback for component "
                           "with UUID {} to driver {}",
                           (uint64_t)id,
                           driver->getName());
                return;
            }
        }

        BESS_WARN("Component with UUID {} not found in any driver. Cannot add "
                  "port count change callback.",
                  (uint64_t)id);
    }

    void SimulationEngine::removeOnPortCountChangeCB(const UUID &id) {
        for (const auto &driver : m_simDrivers) {
            if (driver->hasComponent(id)) {
                driver->removeOnPortCountChangeCB(id);
                BESS_DEBUG("Removed port count change callback for component "
                           "with UUID {} from driver {}",
                           (uint64_t)id,
                           driver->getName());
                return;
            }
        }

        BESS_WARN("Component with UUID {} not found in any driver. Cannot "
                  "remove port count change callback.",
                  (uint64_t)id);
    }

    std::shared_ptr<Drivers::SimDriver>
    SimulationEngine::getDriverWithName(const std::string &name) const {
        for (const auto &driver : m_simDrivers) {
            if (driver->getName() == name) {
                return driver;
            }
        }
        return nullptr;
    }

    Json::Value SimulationEngine::toJson() const {
        std::lock_guard executionLock(m_schedulerExecutionMutex);
        Json::Value json;
        for (const auto &driver : m_simDrivers) {
            json["drivers"][driver->getName()] = driver->toJson();
        }
        return json;
    }

    void SimulationEngine::loadJson(const Json::Value &json) {
        clear();

        if (!json.isObject()) {
            return;
        }

        if (json.isMember("drivers") && json["drivers"].isObject()) {
            for (const auto &driver : m_simDrivers) {
                const auto driverName = driver->getName();
                if (json["drivers"].isMember(driverName)) {
                    driver->loadJson(json["drivers"][driverName]);
                }
            }
        }
    }
} // namespace Bess::SimEngine
