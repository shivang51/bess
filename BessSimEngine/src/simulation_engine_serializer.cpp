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
            SimulationEngine::instance().scheduleEvent(entt, SimulationEngine::instance().currentSimTime);
        }
    }

    void SimEngineSerializer::deserializeFromPath(const std::string &path) {
        auto &registry = SimEngine::SimulationEngine::instance().m_registry;
        registry.clear();
        EnttRegistrySerializer::deserializeFromPath(registry, path);
        simulateClockedComponents();
    }

    nlohmann::json SimEngineSerializer::serialize() {
        return EnttRegistrySerializer::serialize(SimEngine::SimulationEngine::instance().m_registry);
    }

    void SimEngineSerializer::deserialize(const nlohmann::json &json) {
        auto &registry = SimEngine::SimulationEngine::instance().m_registry;
        registry.clear();
        EnttRegistrySerializer::deserialize(registry, json);
        simulateClockedComponents();
    }

    nlohmann::json SimEngineSerializer::serializeEntity(entt::registry &registry, entt::entity entity) {
        nlohmann::json j;

        if (auto *idComp = registry.try_get<IdComponent>(entity)) {
            j["IdComponent"]["uuid"] = static_cast<uint64_t>(idComp->uuid);
        }

        if (auto *gateComp = registry.try_get<DigitalComponent>(entity)) {
            j["DigitalComponent"]["type"] = static_cast<int>(gateComp->type);
            j["DigitalComponent"]["delay"] = gateComp->delay.count();

            for (const auto &input : gateComp->inputPins) {
                nlohmann::json inputJson;
                for (const auto &[id, idx] : input) {
                    inputJson.push_back({{"id", static_cast<uint64_t>(id)}, {"index", idx}});
                }
                j["DigitalComponent"]["inputPins"].push_back(inputJson);
            }

            for (const auto &output : gateComp->outputPins) {
                nlohmann::json outputJson;
                for (const auto &[id, idx] : output) {
                    outputJson.push_back({{"id", static_cast<uint64_t>(id)}, {"index", idx}});
                }
                j["DigitalComponent"]["outputPins"].push_back(outputJson);
            }

            j["DigitalComponent"]["outputStates"] = gateComp->outputStates;
            j["DigitalComponent"]["inputStates"] = gateComp->inputStates;
        }

        if (auto *clockComp = registry.try_get<ClockComponent>(entity)) {
            j["ClockComponent"] = *clockComp;
        }

        if (auto *comp = registry.try_get<FlipFlopComponent>(entity)) {
            j["FlipFlopComponent"] = *comp;
        }
        return j;
    }

    void SimEngineSerializer::deserializeEntity(entt::registry &registry, const nlohmann::json &j) {
        entt::entity entity = registry.create();

        if (j.contains("IdComponent")) {
            auto &idComp = registry.emplace<IdComponent>(entity);
            idComp.uuid = j["IdComponent"]["uuid"].get<uint64_t>();
        }

        if (j.contains("FlipFlopComponent")) {
            registry.emplace<FlipFlopComponent>(entity, j.at("FlipFlopComponent").get<FlipFlopComponent>());
        }

        if (j.contains("DigitalComponent")) {
            auto &gateComp = registry.emplace<DigitalComponent>(entity);
            gateComp.type = static_cast<ComponentType>(j["DigitalComponent"]["type"].get<int>());
            gateComp.delay = SimDelayMilliSeconds(j["DigitalComponent"]["delay"].get<long long>());
            if (j["DigitalComponent"].contains("inputPins")) {
                for (const auto &inputJson : j["DigitalComponent"]["inputPins"]) {
                    std::vector<std::pair<UUID, int>> inputVec;
                    for (const auto &input : inputJson) {
                        inputVec.emplace_back(static_cast<UUID>(input["id"].get<uint64_t>()), input["index"].get<int>());
                    }
                    gateComp.inputPins.push_back(inputVec);
                }
            }

            if (j["DigitalComponent"].contains("outputPins")) {
                for (const auto &outputJson : j["DigitalComponent"]["outputPins"]) {
                    std::vector<std::pair<UUID, int>> outputVec;
                    for (const auto &output : outputJson) {
                        outputVec.emplace_back(static_cast<UUID>(output["id"].get<uint64_t>()), output["index"].get<int>());
                    }
                    gateComp.outputPins.push_back(outputVec);
                }
            }

            gateComp.outputStates = j["DigitalComponent"]["outputStates"].get<std::vector<bool>>();
            gateComp.inputStates = j["DigitalComponent"]["inputStates"].get<std::vector<bool>>();
        }

        if (j.contains("ClockComponent")) {
            auto clockComp = j["ClockComponent"].get<ClockComponent>();
            registry.emplace<ClockComponent>(entity, clockComp);
        }
    }
} // namespace Bess::SimEngine
