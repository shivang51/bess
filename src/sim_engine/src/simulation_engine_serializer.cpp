#include "simulation_engine_serializer.h"
#include "component_catalog.h"
#include "common/logger.h"
#include "simulation_engine.h"

namespace Bess::SimEngine {
    void SimEngineSerializer::serializeToPath(const std::string &path, int indent) {
        // EnttRegistrySerializer::serializeToPath(SimEngine::SimulationEngine::instance().m_registry, path, indent);
    }

    void SimEngineSerializer::simAutoReschedulableComponents() {
        auto &simEngine = SimEngine::SimulationEngine::instance();
        const auto currentTime = simEngine.getSimulationTime();
        for (const auto &[uid, comp] : simEngine.getSimEngineState().getDigitalComponents()) {
            if (comp->definition->getShouldAutoReschedule()) {
                simEngine.scheduleEvent(uid,
                                        UUID::null,
                                        comp->definition->getRescheduleTime(currentTime));
            }
        }
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

        j["analog_components"] = Json::arrayValue;
        for (const auto &component : simEngine.getAnalogCircuit().components()) {
            if (!component) {
                continue;
            }

            const UUID componentId = component->getUUID();
            const auto defIt = simEngine.m_analogComponentDefinitions.find(componentId);
            if (defIt == simEngine.m_analogComponentDefinitions.end() || !defIt->second) {
                BESS_WARN("Skipping analog component {} during serialization: missing definition", (uint64_t)componentId);
                continue;
            }

            if (!defIt->second->isAnalogDefinition()) {
                BESS_WARN("Skipping analog component {} during serialization: definition is not analog", (uint64_t)componentId);
                continue;
            }

            Json::Value compJson = Json::objectValue;
            JsonConvert::toJsonValue(componentId, compJson["id"]);
            compJson["definition_base_hash"] = static_cast<Json::UInt64>(defIt->second->getBaseHash());

            if (const auto numericValue = component->numericValue(); numericValue.has_value()) {
                compJson["numeric_value"] = *numericValue;
            }

            j["analog_components"].append(compJson);
        }

        j["analog_connections"] = Json::arrayValue;
        for (const auto &connection : simEngine.m_analogConnections) {
            Json::Value connectionJson = Json::objectValue;
            JsonConvert::toJsonValue(connection.a.componentId, connectionJson["a_component_id"]);
            connectionJson["a_terminal_idx"] = static_cast<Json::UInt64>(connection.a.terminalIdx);
            JsonConvert::toJsonValue(connection.b.componentId, connectionJson["b_component_id"]);
            connectionJson["b_terminal_idx"] = static_cast<Json::UInt64>(connection.b.terminalIdx);
            j["analog_connections"].append(connectionJson);
        }
    }

    void SimEngineSerializer::serializeEntity(const UUID uid, Json::Value &j) {
        // const auto ent = SimEngine::SimulationEngine::instance().getEntityWithUuid(uid);
        // EnttRegistrySerializer::serializeEntity(SimEngine::SimulationEngine::instance().m_registry, ent, j);
    }

    void SimEngineSerializer::deserialize(const Json::Value &json) {
        auto &simEngine = SimEngine::SimulationEngine::instance();
        JsonConvert::fromJsonValue(json["sim_engine_state"], simEngine.getSimEngineState());

        simEngine.clearAnalogCircuit();

        const auto &catalog = ComponentCatalog::instance();

        if (json.isMember("analog_components") && json["analog_components"].isArray()) {
            for (const auto &compJson : json["analog_components"]) {
                if (!compJson.isObject() ||
                    !compJson.isMember("id") ||
                    !compJson.isMember("definition_base_hash")) {
                    continue;
                }

                UUID componentId = UUID::null;
                JsonConvert::fromJsonValue(compJson["id"], componentId);
                const auto baseHash = compJson["definition_base_hash"].asUInt64();

                if (!catalog.isRegistered(baseHash)) {
                    BESS_WARN("Skipping analog component {} during deserialization: unknown definition hash {}",
                              (uint64_t)componentId, baseHash);
                    continue;
                }

                auto definition = catalog.getComponentDefinition(baseHash)->clone();
                if (!definition || !definition->isAnalogDefinition()) {
                    BESS_WARN("Skipping analog component {} during deserialization: definition is not analog",
                              (uint64_t)componentId);
                    continue;
                }

                auto component = definition->createAnalogComponent();
                if (!component) {
                    BESS_WARN("Skipping analog component {} during deserialization: factory returned null",
                              (uint64_t)componentId);
                    continue;
                }

                component->setUUID(componentId);

                if (compJson.isMember("numeric_value") && compJson["numeric_value"].isNumeric()) {
                    (void)component->setNumericValue(compJson["numeric_value"].asDouble());
                }

                simEngine.addAnalogComponent(component, definition);
            }
        }

        if (json.isMember("analog_connections") && json["analog_connections"].isArray()) {
            for (const auto &connectionJson : json["analog_connections"]) {
                if (!connectionJson.isObject() ||
                    !connectionJson.isMember("a_component_id") ||
                    !connectionJson.isMember("a_terminal_idx") ||
                    !connectionJson.isMember("b_component_id") ||
                    !connectionJson.isMember("b_terminal_idx")) {
                    continue;
                }

                UUID aComponentId = UUID::null;
                UUID bComponentId = UUID::null;
                JsonConvert::fromJsonValue(connectionJson["a_component_id"], aComponentId);
                JsonConvert::fromJsonValue(connectionJson["b_component_id"], bComponentId);

                const auto endpointA = SimulationEngine::SlotEndpoint{
                    .componentId = aComponentId,
                    .slotIdx = static_cast<int>(connectionJson["a_terminal_idx"].asUInt()),
                    .slotType = SlotType::analogTerminal,
                };
                const auto endpointB = SimulationEngine::SlotEndpoint{
                    .componentId = bComponentId,
                    .slotIdx = static_cast<int>(connectionJson["b_terminal_idx"].asUInt()),
                    .slotType = SlotType::analogTerminal,
                };

                if (!simEngine.connectSlots(endpointA, endpointB)) {
                    BESS_WARN("Failed to restore analog connection {}:{} <-> {}:{}",
                              (uint64_t)aComponentId,
                              endpointA.slotIdx,
                              (uint64_t)bComponentId,
                              endpointB.slotIdx);
                }
            }
        }

        simAutoReschedulableComponents();
    }

    void SimEngineSerializer::deserializeEntity(const Json::Value &json) {
        // auto &registry = SimEngine::SimulationEngine::instance().m_registry;
        // EnttRegistrySerializer::deserializeEntity(registry, json);
    }

} // namespace Bess::SimEngine
