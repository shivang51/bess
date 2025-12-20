#include "sim_engine_state.h"

namespace Bess::SimEngine {
    SimEngineState::SimEngineState() = default;

    SimEngineState::~SimEngineState() = default;

    void SimEngineState::addDigitalComponent(const std::shared_ptr<DigitalComponent> &comp) {
        m_digitalComponents[comp->id] = comp;
    }

    void SimEngineState::removeDigitalComponent(const UUID &uuid) {
        m_digitalComponents.erase(uuid);
    }

    const std::unordered_map<UUID, std::shared_ptr<DigitalComponent>> &SimEngineState::getDigitalComponents() const {
        return m_digitalComponents;
    }

    void SimEngineState::clearDigitalComponents() {
        m_digitalComponents.clear();
    }

    std::shared_ptr<DigitalComponent> SimEngineState::getDigitalComponent(const UUID &uuid) const {
        auto it = m_digitalComponents.find(uuid);
        if (it != m_digitalComponents.end()) {
            return it->second;
        }
        return nullptr;
    }

    void SimEngineState::addNet(const Net &net) {
        m_nets[net.getUUID()] = net;
    }

    void SimEngineState::removeNet(const UUID &uuid) {
        m_nets.erase(uuid);
    }

    const std::unordered_map<UUID, Net> &SimEngineState::getNetsMap() const {
        return m_nets;
    }

    void SimEngineState::clearNets() {
        m_nets.clear();
    }

    void SimEngineState::reset() {
        clearNets();
        clearDigitalComponents();
    }

    bool SimEngineState::isComponentValid(const UUID &uuid) const {
        return m_digitalComponents.contains(uuid);
    }
} // namespace Bess::SimEngine
