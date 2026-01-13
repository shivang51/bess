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
        const auto &simEngine = SimEngine::SimulationEngine::instance();
        j = Json::Value(Json::objectValue);
        JsonConvert::toJsonValue(simEngine.getSimEngineState(), j["sim_engine_state"]);
    }

    void SimEngineSerializer::serializeEntity(const UUID uid, Json::Value &j) {
        // const auto ent = SimEngine::SimulationEngine::instance().getEntityWithUuid(uid);
        // EnttRegistrySerializer::serializeEntity(SimEngine::SimulationEngine::instance().m_registry, ent, j);
    }

    void SimEngineSerializer::deserialize(const Json::Value &json) {
        auto &simEngine = SimEngine::SimulationEngine::instance();
        JsonConvert::fromJsonValue(json["sim_engine_state"], simEngine.getSimEngineState());
        simulateClockedComponents();
    }

    void SimEngineSerializer::deserializeEntity(const Json::Value &json) {
        // auto &registry = SimEngine::SimulationEngine::instance().m_registry;
        // EnttRegistrySerializer::deserializeEntity(registry, json);
    }

} // namespace Bess::SimEngine
