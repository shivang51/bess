#include "simulation_engine_serializer.h"
#include "entt_components.h"
#include "simulation_engine.h"
#include <iostream>

namespace Bess::SimEngine {
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

    void SimEngineSerializer::serialize(nlohmann::json& j) {
        EnttRegistrySerializer::serialize(SimEngine::SimulationEngine::instance().m_registry, j);
    }

    void SimEngineSerializer::deserialize(const nlohmann::json &json) {
        auto &registry = SimEngine::SimulationEngine::instance().m_registry;
        registry.clear();
        EnttRegistrySerializer::deserialize(registry, json);
        simulateClockedComponents();
    }

    void SimEngineSerializer::registerAll() {
        registerComponent<IdComponent>("IdComponent");
        registerComponent<DigitalComponent>("DigitalComponent");
        registerComponent<ClockComponent>("ClockComponent");
        registerComponent<FlipFlopComponent>("FlipFlopComponent");
    }
} // namespace Bess::SimEngine
