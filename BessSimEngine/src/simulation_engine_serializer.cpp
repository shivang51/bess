#include "simulation_engine_serializer.h"
#include "entt_components.h"
#include "simulation_engine.h"
#include <iostream>

namespace Bess::SimEngine {
    void SimEngineSerializer::serializeToPath(const std::string &path, int indent) {
        EnttRegistrySerializer::serializeToPath(SimEngine::SimulationEngine::instance().m_registry, path, indent);
    }

    void SimEngineSerializer::deserializeFromPath(const std::string &path) {
        auto &registry = SimEngine::SimulationEngine::instance().m_registry;
        registry.clear();
        EnttRegistrySerializer::deserializeFromPath(registry, path);
    }

    std::string SimEngineSerializer::serialize(int indent) {
        return EnttRegistrySerializer::serialize(SimEngine::SimulationEngine::instance().m_registry, indent);
    }

    void SimEngineSerializer::deserialize(const std::string &json) {
        auto &registry = SimEngine::SimulationEngine::instance().m_registry;
        registry.clear();
        EnttRegistrySerializer::deserialize(registry, json);
    }

    nlohmann::json SimEngineSerializer::serializeEntity(entt::registry &registry, entt::entity entity) {
        nlohmann::json j;

        if (auto *idComp = registry.try_get<IdComponent>(entity)) {
            j["IdComponent"]["uuid"] = static_cast<uint64_t>(idComp->uuid);
        }

        if (auto *gateComp = registry.try_get<GateComponent>(entity)) {
            j["GateComponent"]["type"] = static_cast<int>(gateComp->type);
            j["GateComponent"]["delay"] = gateComp->delay.count();

            for (const auto &input : gateComp->inputPins) {
                nlohmann::json inputJson;
                for (const auto &[id, idx] : input) {
                    inputJson.push_back({{"id", static_cast<uint64_t>(id)}, {"index", idx}});
                }
                j["GateComponent"]["inputPins"].push_back(inputJson);
            }

            for (const auto &output : gateComp->outputPins) {
                nlohmann::json outputJson;
                for (const auto &[id, idx] : output) {
                    outputJson.push_back({{"id", static_cast<uint64_t>(id)}, {"index", idx}});
                }
                j["GateComponent"]["outputPins"].push_back(outputJson);
            }

            j["GateComponent"]["outputStates"] = gateComp->outputStates;
        }

        return j;
    }

    void SimEngineSerializer::deserializeEntity(entt::registry &registry, const nlohmann::json &j) {
        entt::entity entity = registry.create();

        std::cout << "[SimEngine] Creating entity " << (uint64_t)entity;

        if (j.contains("IdComponent")) {
            auto &idComp = registry.emplace<IdComponent>(entity);
            idComp.uuid = j["IdComponent"]["uuid"].get<uint64_t>();
            std::cout << " with id " << idComp.uuid << std::endl;
        }

        if (j.contains("GateComponent")) {
            auto &gateComp = registry.emplace<GateComponent>(entity);
            gateComp.type = static_cast<ComponentType>(j["GateComponent"]["type"].get<int>());
            gateComp.delay = SimDelayMilliSeconds(j["GateComponent"]["delay"].get<long long>());
            if (j["GateComponent"].contains("inputPins")) {
                for (const auto &inputJson : j["GateComponent"]["inputPins"]) {
                    std::vector<std::pair<UUID, int>> inputVec;
                    for (const auto &input : inputJson) {
                        inputVec.emplace_back(static_cast<UUID>(input["id"].get<uint64_t>()), input["index"].get<int>());
                    }
                    gateComp.inputPins.push_back(inputVec);
                }
            }

            if (j["GateComponent"].contains("outputPins")) {
                for (const auto &outputJson : j["GateComponent"]["outputPins"]) {
                    std::vector<std::pair<UUID, int>> outputVec;
                    for (const auto &output : outputJson) {
                        outputVec.emplace_back(static_cast<UUID>(output["id"].get<uint64_t>()), output["index"].get<int>());
                    }
                    gateComp.outputPins.push_back(outputVec);
                }
            }

            gateComp.outputStates = j["GateComponent"]["outputStates"].get<std::vector<bool>>();
        }
    }
} // namespace Bess::SimEngine
