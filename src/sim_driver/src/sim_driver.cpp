#include "sim_driver/sim_driver.h"
#include "common/bess_uuid.h"
#include "common/logger.h"
#include "common/types.h"

namespace Bess::SimEngine::Drivers {

    Json::Value CompDef::toJson() const {
        Json::Value json;
        json["groupName"] = m_groupName;
        json["typeName"] = getTypeName();
        json["name"] = m_name;
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
        if (isRunning()) {
            {
                std::lock_guard lk(m_stateMutex);
                m_state = SimDriverState::paused;
            }
            onPause();
        }
    }

    void SimDriver::resume() {
        if (isPaused()) {
            {
                std::lock_guard lk(m_stateMutex);
                m_state = SimDriverState::running;
            }
            onResume();
        }
    }

    void SimDriver::stop() {
        if (isRunning() || isPaused()) {
            {
                std::lock_guard lk(m_stateMutex);
                m_state = SimDriverState::stopped;
            }
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
} // namespace Bess::SimEngine::Drivers
