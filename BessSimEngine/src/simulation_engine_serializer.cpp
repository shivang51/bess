#include "simulation_engine_serializer.h"
#include "entt_components.h"
#include "simulation_engine.h"

namespace Bess::SimEngine {
    void SimEngineSerializer::serializeToPath(const std::string &path, int indent) {
        EnttRegistrySerializer::serializeToPath(SimEngine::SimulationEngine::instance().m_registry, path, indent);
    }

    std::string SimEngineSerializer::serialize(int indent) {
        return EnttRegistrySerializer::serialize(SimEngine::SimulationEngine::instance().m_registry, indent);
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
                for (const auto &[ent, idx] : input) {
                    inputJson.push_back({{"entity", static_cast<uint32_t>(ent)}, {"index", idx}});
                }
                j["GateComponent"]["inputPins"].push_back(inputJson);
            }

            for (const auto &output : gateComp->outputPins) {
                nlohmann::json outputJson;
                for (const auto &[ent, idx] : output) {
                    outputJson.push_back({{"entity", static_cast<uint32_t>(ent)}, {"index", idx}});
                }
                j["GateComponent"]["outputPins"].push_back(outputJson);
            }

            j["GateComponent"]["outputStates"] = gateComp->outputStates;
        }

        return j;
    }

    void SimEngineSerializer::deserializeEntity(entt::registry &registry, const nlohmann::json &j) {
        entt::entity entity = registry.create();

        if (j.contains("IdComponent")) {
            auto &idComp = registry.emplace<IdComponent>(entity);
            idComp.uuid = j["IdComponent"]["uuid"].get<uint64_t>();
        }

        if (j.contains("GateComponent")) {
            auto &gateComp = registry.emplace<GateComponent>(entity);
            gateComp.type = static_cast<ComponentType>(j["GateComponent"]["type"].get<int>());
            gateComp.delay = SimDelayMilliSeconds(j["GateComponent"]["delay"].get<long long>());

            for (const auto &inputJson : j["GateComponent"]["inputPins"]) {
                std::vector<std::pair<entt::entity, int>> inputVec;
                for (const auto &input : inputJson) {
                    inputVec.emplace_back(static_cast<entt::entity>(input["entity"].get<uint32_t>()), input["index"].get<int>());
                }
                gateComp.inputPins.push_back(inputVec);
            }

            for (const auto &outputJson : j["GateComponent"]["outputPins"]) {
                std::vector<std::pair<entt::entity, int>> outputVec;
                for (const auto &output : outputJson) {
                    outputVec.emplace_back(static_cast<entt::entity>(output["entity"].get<uint32_t>()), output["index"].get<int>());
                }
                gateComp.outputPins.push_back(outputVec);
            }

            gateComp.outputStates = j["GateComponent"]["outputStates"].get<std::vector<bool>>();
        }
    }
} // namespace Bess::SimEngine
