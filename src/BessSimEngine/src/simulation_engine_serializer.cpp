#include "simulation_engine_serializer.h"
#include "simulation_engine.h"

namespace Bess::SimEngine {
    void SimEngineSerializer::serializeToPath(const std::string &path, int indent) {
        // EnttRegistrySerializer::serializeToPath(SimEngine::SimulationEngine::instance().m_registry, path, indent);
    }

    void SimEngineSerializer::simulateClockedComponents() {
        // const auto &registry = SimEngine::SimulationEngine::instance().m_registry;
        // auto view = registry.view<SimEngine::ClockComponent>();
        //
        // for (auto entt : view) {
        //     SimulationEngine::instance().scheduleEvent(entt, entt::null, SimulationEngine::instance().m_currentSimTime);
        // }
    }

    void SimEngineSerializer::deserializeFromPath(const std::string &path) {
        // auto &registry = SimEngine::SimulationEngine::instance().m_registry;
        // registry.clear();
        // EnttRegistrySerializer::deserializeFromPath(registry, path);
        // simulateClockedComponents();
    }

    void SimEngineSerializer::serialize(Json::Value &j) {
        // Json::Value registryJson;
        const auto &simEngine = SimEngine::SimulationEngine::instance();
        // EnttRegistrySerializer::serialize(simEngine.m_registry, registryJson);
        //
        // j["registry"] = registryJson;
        j["nets"] = Json::arrayValue;
        for (const auto &[netUuid, net] : simEngine.m_nets) {
            Json::Value netJson;
            JsonConvert::toJsonValue(netJson, net);
            j["nets"].append(netJson);
        }
    }

    void SimEngineSerializer::serializeEntity(const UUID uid, Json::Value &j) {
        // const auto ent = SimEngine::SimulationEngine::instance().getEntityWithUuid(uid);
        // EnttRegistrySerializer::serializeEntity(SimEngine::SimulationEngine::instance().m_registry, ent, j);
    }

    void SimEngineSerializer::deserialize(const Json::Value &json) {
        // auto &simEngine = SimEngine::SimulationEngine::instance();
        // auto &registry = simEngine.m_registry;
        // registry.clear();
        // EnttRegistrySerializer::deserialize(registry, json["registry"]);
        //
        // simEngine.m_nets.clear();
        // for (const auto &netJson : json["nets"]) {
        //     Net net;
        //     JsonConvert::fromJsonValue(netJson, net);
        //     simEngine.m_nets[net.getUUID()] = net;
        // }
        //
        // simulateClockedComponents();
    }

    void SimEngineSerializer::deserializeEntity(const Json::Value &json) {
        // auto &registry = SimEngine::SimulationEngine::instance().m_registry;
        // EnttRegistrySerializer::deserializeEntity(registry, json);
    }

} // namespace Bess::SimEngine
