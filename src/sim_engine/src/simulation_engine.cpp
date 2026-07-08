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

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

// #define BESS_ENABLE_LOG_EVENTS

#ifdef BESS_ENABLE_LOG_EVENTS
    #define BESS_LOG_EVENT(...) BESS_TRACE(__VA_ARGS__);
#else
    #define BESS_LOG_EVENT(...)
#endif // !BESS_LOG_EVENT

namespace Bess::SimEngine {
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

    SimulationEngine::SimulationEngine() = default;

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

        stop();

        destroyDrivers();
        unloadDrivers();

        m_stateCV.notify_all();

        ComponentCatalog::instance().destroy();

        m_destroyed = true;
    }

    const UUID &SimulationEngine::addComponent(
        const std::shared_ptr<Drivers::CompDef> &definition, bool cloneDef) {
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

        m_pendingSignalSources.erase(uuid);

        BESS_INFO("Deleted component {}", (uint64_t)uuid);
    }

    SlotState SimulationEngine::getPortState(const PortRef &port) {
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

    void SimulationEngine::setInputSlotState(const UUID &uuid,
                                             int pinIdx,
                                             LogicState state) {
        for (const auto &driver : m_simDrivers) {
            if (driver->hasComponent(uuid)) {
                driver->setInputSlotState(uuid, pinIdx, state);
                return;
            }
        }

        BESS_WARN("[setInputSlotState] Component with UUID {} is invalid",
                  (uint64_t)uuid);
    }

    void SimulationEngine::setOutputSlotState(const UUID &uuid,
                                              int pinIdx,
                                              LogicState state) {
        for (const auto &driver : m_simDrivers) {
            if (driver->hasComponent(uuid)) {
                driver->setOutputSlotState(uuid, pinIdx, state);
                return;
            }
        }

        BESS_WARN("[setOutputSlotState] Component with UUID {} is invalid",
                  (uint64_t)uuid);
    }

    const ComponentState &
    SimulationEngine::getComponentState(const UUID &uuid) {
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

    std::vector<SlotState>
    SimulationEngine::getInputSlotsState(UUID compId) const {
        for (const auto &driver : m_simDrivers) {
            if (driver->hasComponent(compId)) {
                return driver->getInputSlotsState(compId);
            }
        }

        return {};
    }

    SimulationState SimulationEngine::getSimulationState() const {
        return m_simState.load();
    }

    void SimulationEngine::stepSimulation() {
        std::unique_lock stateLock(m_stateMutex);
        if (m_simState.load() != SimulationState::paused || m_stepFlag.load())
            return;
        m_stepFlag.store(true);
        m_stateCV.notify_all();
        stateLock.unlock();

        for (auto &driver : m_simDrivers) {
            driver->step();
        }

        stateLock.lock();
        m_stepFlag.store(false);
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
        std::unique_lock stateLock(m_stateMutex);
        SimulationState prevState = m_simState.load();
        m_simState.store(state);
        m_runCtx.simState = state;
        m_stateCV.notify_all();
        stateLock.unlock();

        if (state == SimulationState::running) {
            if (prevState == SimulationState::paused) {
                resumeDrivers();
                processPendingPropagation();
            } else {
                runDrivers();
            }
        } else if (state == SimulationState::stopped) {
            stopDrivers();
        } else if (state == SimulationState::paused) {
            pauseDrivers();
        }
    }

    void SimulationEngine::clearPendingDriverEvents() {
        for (auto &driver : m_simDrivers) {
            driver->clearPendingEvents();
        }
    }

    void SimulationEngine::run() {
        m_runCtx.elapsedTime = TimeMs(0);
        m_runCtx.runDuration = TimeMs(0);
        m_runCtx.stepInterval = TimeMs(0);
        m_runCtx.isTimedRun = false;
        setSimulationState(SimulationState::running);
    }

    void SimulationEngine::runFor(TimeMs duration, TimeMs stepInterval) {
        const auto startTime = std::chrono::steady_clock::now();
        const auto endTime = startTime + duration;
        m_runCtx.runDuration = duration;
        m_runCtx.stepInterval = stepInterval;
        m_runCtx.elapsedTime = TimeMs(0);
        m_runCtx.isTimedRun = true;

        if (m_timedRunThread.joinable()) {
            BESS_WARN("Joining timed run thread before new run");
            m_timedRunThread.join();
        }

        // Do the intial stamping
        stampSim(TimeMs(0));
        setSimulationState(SimulationState::running);

        auto runFn = [this, startTime, endTime, stepInterval]() {
            while (std::chrono::steady_clock::now() <= endTime &&
                   (getSimulationState() == SimulationState::paused ||
                    getSimulationState() == SimulationState::running)) {
                if (stepInterval.count() > 0) {
                    std::this_thread::sleep_for(stepInterval);
                }
                m_runCtx.elapsedTime =
                    std::chrono::steady_clock::now() - startTime;
                stampSim(m_runCtx.elapsedTime);
            }

            m_runCtx.elapsedTime = std::chrono::steady_clock::now() - startTime;
            setSimulationState(SimulationState::stopped);
        };

        m_timedRunThread = std::thread(runFn);
    }

    void SimulationEngine::stop() {
        setSimulationState(SimulationState::stopped);
        if (m_timedRunThread.joinable()) {
            BESS_DEBUG("Joining timed run thread");
            m_timedRunThread.join();
        }
    }

    bool SimulationEngine::isNetUpdated() const {
        for (const auto &driver : m_simDrivers) {
            if (driver->isNetUpdated()) {
                return true;
            }
        }

        return false;
    }

    std::unordered_map<UUID, Net> SimulationEngine::getNetsMap(bool update) {
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
        for (const auto &driver : m_simDrivers) {
            if (driver->hasComponent(sourceId)) {
                driver->propagateFromComponent(sourceId);
                return;
            }
        }
    }

    void SimulationEngine::markPendingSignalSource(const UUID &sourceId) {
        m_pendingSignalSources.insert(sourceId);
    }

    bool SimulationEngine::addPort(const PortRef &port, bool force) {
        for (const auto &driver : m_simDrivers) {
            if (driver->hasComponent(port.componentId)) {
                const auto res = driver->addPort(port, force);

                if (!res.hasChange())
                    return false;

                if (res.changedInp) {
                    Events::CompDefInputsResizedEvent event{port.componentId};
                    auto &appCtx = GAppContext::getInstance();
                    auto eventDispatcher =
                        appCtx
                            .getSubSystem<Bess::EventSystem::EventDispatcher>();
                    eventDispatcher->queue(event);
                }

                if (res.changedOut) {
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
        for (const auto &driver : m_simDrivers) {
            if (driver->hasComponent(port.componentId)) {
                const auto res = driver->removePort(port, force);

                if (!res.hasChange())
                    return false;

                if (res.changedInp) {
                    Events::CompDefInputsResizedEvent event{port.componentId};
                    auto &appCtx = GAppContext::getInstance();
                    auto eventDispatcher =
                        appCtx
                            .getSubSystem<Bess::EventSystem::EventDispatcher>();
                    eventDispatcher->queue(event);
                }

                if (res.changedOut) {
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
            std::lock_guard lk(m_driversMutex);
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
                m_simDrivers.push_back(driver);
                BESS_INFO("Loaded driver {}", driver->getName());
            } else {
                BESS_ERROR("Failed to load driver {}", name);
            }
        }
    }

    void SimulationEngine::unloadDrivers() {
        const size_t n = m_simDrivers.size();
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

        m_driverThreads.clear();
    }

    void SimulationEngine::runDrivers() {
        for (auto &driver : m_simDrivers) {
            m_driverThreads.emplace_back([driver]() { driver->run(); });
            BESS_INFO("Started driver thread for {}", driver->getName());
        }
    }

    void SimulationEngine::stopDrivers() {
        for (auto &driver : m_simDrivers) {
            driver->stop();
            BESS_INFO("Stopped driver {}", driver->getName());
        }

        for (auto &thread : m_driverThreads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }

    void SimulationEngine::pauseDrivers() {
        for (auto &driver : m_simDrivers) {
            driver->pause();
            BESS_INFO("Paused driver {}", driver->getName());
        }
    }

    void SimulationEngine::resumeDrivers() {
        for (auto &driver : m_simDrivers) {
            driver->resume();
            BESS_INFO("Resumed driver {}", driver->getName());
        }
    }

    void SimulationEngine::stampSim(TimeMs elapsedTime) {
        for (const auto &driver : m_simDrivers) {
            driver->stampSim(elapsedTime);
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
