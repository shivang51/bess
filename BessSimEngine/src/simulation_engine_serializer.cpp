#include "simulation_engine_serializer.h"
#include "entt_components.h"
#include "simulation_engine.h"
#include <iostream>

namespace Bess::SimEngine {
    SimEngineSerializer::SimEngineSerializer() {
        SimEngineSerializer::registerAll();
    }

    void SimEngineSerializer::serializeToPath(const std::string &path, int indent) {
        EnttRegistrySerializer::serializeToPath(SimEngine::SimulationEngine::instance().m_registry, path, indent);
    }

    void SimEngineSerializer::simulateClockedComponents() {
        const auto &registry = SimEngine::SimulationEngine::instance().m_registry;
        auto view = registry.view<SimEngine::ClockComponent>();

        for (auto entt : view) {
            SimulationEngine::instance().scheduleEvent(entt, entt::null, SimulationEngine::instance().m_currentSimTime);
        }
    }

    void SimEngineSerializer::deserializeFromPath(const std::string &path) {
        auto &registry = SimEngine::SimulationEngine::instance().m_registry;
        registry.clear();
        EnttRegistrySerializer::deserializeFromPath(registry, path);
        simulateClockedComponents();
    }

    void SimEngineSerializer::serialize(Json::Value &j) {
        EnttRegistrySerializer::serialize(SimEngine::SimulationEngine::instance().m_registry, j);
    }

    void SimEngineSerializer::serializeEntity(const UUID uid, Json::Value &j) {
        const auto ent = SimEngine::SimulationEngine::instance().getEntityWithUuid(uid);
        EnttRegistrySerializer::serializeEntity(SimEngine::SimulationEngine::instance().m_registry, ent, j);
    }

    void SimEngineSerializer::deserialize(const Json::Value &json) {
        auto &registry = SimEngine::SimulationEngine::instance().m_registry;
        registry.clear();
        EnttRegistrySerializer::deserialize(registry, json);
        simulateClockedComponents();
    }

    void SimEngineSerializer::deserializeEntity(const Json::Value &json) {
        auto &registry = SimEngine::SimulationEngine::instance().m_registry;
        EnttRegistrySerializer::deserializeEntity(registry, json);
    }

    void SimEngineSerializer::registerAll() {
        registerComponent<IdComponent>("IdComponent");
        // registerComponent<DigitalComponent>("DigitalComponent");
        registerComponent<ClockComponent>("ClockComponent");
        registerComponent<FlipFlopComponent>("FlipFlopComponent");
    }
} // namespace Bess::SimEngine
