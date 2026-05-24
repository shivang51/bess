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
    SimulationEngine &SimulationEngine::instance() {
        static SimulationEngine inst;
        return inst;
    }

    SimulationEngine::SimulationEngine() {
        loadDrivers();
        initDrivers();

        const auto &pluginMangaer = Plugins::PluginManager::getInstance();

        auto &catalog = ComponentCatalog::instance();
        for (const auto &plugin : pluginMangaer.getLoadedPlugins()) {
            const auto comps = plugin.second->onCompCatalogLoad();
            for (const auto &comp : comps) {
                catalog.registerComponent(comp);
            }
            BESS_INFO("Registered {} components from plugin {}", comps.size(),
                      plugin.first);
        }
        Plugins::savePyThreadState();
        m_simThread = std::thread(&SimulationEngine::run, this);
    }

    void SimulationEngine::clear() {
        const auto previousState = getSimulationState();
        setSimulationState(SimulationState::paused);
        clearPendingDriverEvents();

        for (auto &driver : m_simDrivers) {
            driver->clearComponents();
        }

        {
            std::lock_guard lkRegistry(m_pendingSignalSourcesMutex);
            m_pendingSignalSources.clear();
        }

        if (previousState == SimulationState::running) {
            setSimulationState(SimulationState::running);
        }
    }

    SimulationEngine::~SimulationEngine() { destroy(); }

    void SimulationEngine::destroy() {
        if (m_destroyed)
            return;

        clear();

        stopDrivers();
        destroyDrivers();
        unloadDrivers();

        m_stopFlag.store(true);
        m_stateCV.notify_all();
        if (m_simThread.joinable())
            m_simThread.join();

        Plugins::restorePyThreadState();
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

    std::pair<bool, std::string> SimulationEngine::canConnectComponents(
        const UUID &src, int srcSlot, SlotType srcType, const UUID &dst,
        int dstSlot, SlotType dstType) const {
        if (src == UUID::null || dst == UUID::null) {
            return {false, "Cannot connect to/from null component"};
        }

        std::shared_ptr<Drivers::SimDriver> srcDriver, dstDriver;
        for (const auto &driver : m_simDrivers) {
            if (driver->hasComponent(src))
                srcDriver = driver;
            if (driver->hasComponent(dst))
                dstDriver = driver;
        }

        if (!srcDriver || !dstDriver) {
            return {
                false,
                "Source or destination component does not exist in any driver"};
        }

        if (srcDriver != dstDriver) {
            return {false, "Cross-driver connection is not currently supported "
                           "generically"};
        }

        return srcDriver->canConnectComponents(src, srcSlot, srcType, dst,
                                               dstSlot, dstType);
    }

    bool SimulationEngine::connectComponent(const UUID &src, int srcSlot,
                                            SlotType srcType, const UUID &dst,
                                            int dstSlot, SlotType dstType,
                                            bool overrideConn) {
        std::shared_ptr<Drivers::SimDriver> srcDriver, dstDriver;
        for (const auto &driver : m_simDrivers) {
            if (driver->hasComponent(src))
                srcDriver = driver;
            if (driver->hasComponent(dst))
                dstDriver = driver;
        }

        if (!srcDriver || !dstDriver || srcDriver != dstDriver) {
            return false;
        }

        std::lock_guard lk(m_driversMutex);
        return srcDriver->connectComponent(src, srcSlot, srcType, dst, dstSlot,
                                           dstType, overrideConn);
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

    SlotState SimulationEngine::getDigitalSlotState(const UUID &uuid,
                                                    SlotType type, int idx) {
        if (!getComponentDefinition(uuid)) {
            BESS_WARN("[getDigitalPinState] Component with UUID {} is invalid",
                      (uint64_t)uuid);
            return {LogicState::unknown, SimTime(0)};
        }

        for (const auto &driver : m_simDrivers) {
            if (driver->hasComponent(uuid)) {
                return driver->getSlotState(uuid, type, idx);
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

    void SimulationEngine::setInputSlotState(const UUID &uuid, int pinIdx,
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

    void SimulationEngine::setOutputSlotState(const UUID &uuid, int pinIdx,
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
        const auto &comp = getComponent<Drivers::SimComponent>(uuid);
        return comp->getDefinition();
    }

    void SimulationEngine::deleteConnection(const UUID &compA,
                                            SlotType pinAType, int idxA,
                                            const UUID &compB,
                                            SlotType pinBType, int idxB) {
        std::shared_ptr<Drivers::SimDriver> aDriver, bDriver;
        for (const auto &driver : m_simDrivers) {
            if (driver->hasComponent(compA))
                aDriver = driver;
            if (driver->hasComponent(compB))
                bDriver = driver;
        }

        if (!aDriver || !bDriver || aDriver != bDriver) {
            return;
        }

        std::lock_guard lk(m_driversMutex);
        aDriver->deleteConnection(compA, pinAType, idxA, compB, pinBType, idxB);
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

    SimulationState SimulationEngine::toggleSimState() {
        if (m_simState == SimulationState::paused) {
            setSimulationState(SimulationState::running);
        } else if (m_simState == SimulationState::running) {
            setSimulationState(SimulationState::paused);
        }

        return m_simState.load();
    }

    void SimulationEngine::setSimulationState(SimulationState state) {
        std::unique_lock stateLock(m_stateMutex);
        m_simState.store(state);
        m_stateCV.notify_all();
        stateLock.unlock();

        for (auto &driver : m_simDrivers) {
            if (state == SimulationState::paused) {
                driver->pause();
            } else if (state == SimulationState::running) {
                driver->resume();
            }
        }

        if (state == SimulationState::running) {
            processPendingPropagation();
        }
    }

    void SimulationEngine::clearPendingDriverEvents() {
        for (auto &driver : m_simDrivers) {
            driver->clearPendingEvents();
        }
    }

    void SimulationEngine::run() {
        runDrivers();
        while (!m_stopFlag.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
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

    bool SimulationEngine::addSlot(const UUID &compId, SlotType type, int index,
                                   bool force) {
        for (const auto &driver : m_simDrivers) {
            if (driver->hasComponent(compId)) {
                const auto res = driver->addSlot(compId, type, index, force);

                if (!res.hasChange())
                    return false;

                if (res.changedInp) {
                    Events::CompDefInputsResizedEvent event{compId};
                    EventSystem::EventDispatcher::instance().queue(event);
                }

                if (res.changedOut) {
                    Events::CompDefOutputsResizedEvent event{compId};
                    EventSystem::EventDispatcher::instance().queue(event);
                }

                return true;
            }
        }
        return false;
    }

    bool SimulationEngine::removeSlot(const UUID &compId, SlotType type,
                                      int index, bool force) {
        for (const auto &driver : m_simDrivers) {
            if (driver->hasComponent(compId)) {
                const auto res = driver->removeSlot(compId, type, index, force);

                if (!res.hasChange())
                    return false;

                if (res.changedInp) {
                    Events::CompDefInputsResizedEvent event{compId};
                    EventSystem::EventDispatcher::instance().queue(event);
                }

                if (res.changedOut) {
                    Events::CompDefOutputsResizedEvent event{compId};
                    EventSystem::EventDispatcher::instance().queue(event);
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

    void SimulationEngine::addOnSlotCountChangeCB(
        const UUID &id, const Drivers::SlotCountChangeCB &cb) {
        for (const auto &driver : m_simDrivers) {
            if (driver->hasComponent(id)) {
                driver->addOnSlotCountChangeCB(id, cb);
                BESS_DEBUG("Added slot count change callback for component "
                           "with UUID {} to driver {}",
                           (uint64_t)id, driver->getName());
                return;
            }
        }

        BESS_WARN("Component with UUID {} not found in any driver. Cannot add "
                  "slot count change callback.",
                  (uint64_t)id);
    }

    void SimulationEngine::removeOnSlotCountChangeCB(const UUID &id) {
        for (const auto &driver : m_simDrivers) {
            if (driver->hasComponent(id)) {
                driver->removeOnSlotCountChangeCB(id);
                BESS_DEBUG("Removed slot count change callback for component "
                           "with UUID {} from driver {}",
                           (uint64_t)id, driver->getName());
                return;
            }
        }

        BESS_WARN("Component with UUID {} not found in any driver. Cannot "
                  "remove slot count change callback.",
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
