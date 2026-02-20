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

namespace Bess::JsonConvert {
    void toJsonValue(const Bess::SimEngine::SimEngineState &state, Json::Value &j) {
        j["digital_components"] = Json::arrayValue;
        for (const auto &[uuid, comp] : state.getDigitalComponents()) {
            Json::Value compJson;
            JsonConvert::toJsonValue(compJson, *comp);
            j["digital_components"].append(compJson);
        }

        j["nets"] = Json::arrayValue;
        for (const auto &[netUuid, net] : state.getNetsMap()) {
            JsonConvert::toJsonValue(net, j["nets"].append(Json::Value()));
        }
    }

    void fromJsonValue(const Json::Value &j, Bess::SimEngine::SimEngineState &state) {
        state.reset();

        for (const auto &compJson : j["digital_components"]) {
            std::shared_ptr<SimEngine::DigitalComponent> comp = std::make_shared<SimEngine::DigitalComponent>();
            JsonConvert::fromJsonValue(compJson, *comp);
            state.addDigitalComponent(comp);
        }

        for (const auto &netJson : j["nets"]) {
            SimEngine::Net net;
            JsonConvert::fromJsonValue(netJson, net);
            state.addNet(net);
        }
    }
} // namespace Bess::JsonConvert
