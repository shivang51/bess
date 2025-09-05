#pragma once
#include "bess_api.h"
#include "bess_uuid.h"
#include "component_catalog.h"
#include "entt_components.h"
#include "types.h"
#include "json/json.h"

namespace Bess::JsonConvert {
    // --- Bess::UUID ---
    BESS_API inline void toJsonValue(const Bess::UUID &uuid, Json::Value &j) {
        j = (Json::UInt64)uuid;
    }

    BESS_API inline void fromJsonValue(const Json::Value &j, Bess::UUID &uuid) {
        uuid = j.asUInt64();
    }

    using namespace Bess::SimEngine;

    // --- IdComponent ---

    inline void toJsonValue(const IdComponent &comp, Json::Value &j) {
        j = Json::Value(Json::objectValue);
        j["uuid"] = static_cast<Json::UInt64>(comp.uuid);
    }

    inline void fromJsonValue(const Json::Value &j, IdComponent &comp) {
        if (j.isMember("uuid")) {
            comp.uuid = static_cast<UUID>(j["uuid"].asUInt64());
        }
    }

    // --- FlipFlopComonent ---

    inline void toJsonValue(const FlipFlopComponent &comp, Json::Value &j) {
        j = Json::Value(Json::objectValue);
        j["type"] = (int)comp.type;
    }

    inline void fromJsonValue(const Json::Value &j, FlipFlopComponent &comp) {
        if (j.isMember("type")) {
            comp.type = (ComponentType)j["type"].asInt();
        }
    }

    // --- ClockComponent ---

    inline void toJsonValue(const ClockComponent &comp, Json::Value &j) {
        j = Json::Value(Json::objectValue);
        j["frequency"] = comp.frequency;
        j["frequencyUnit"] = (int)comp.frequencyUnit;
        j["dutyCycle"] = comp.dutyCycle;
    }

    inline void fromJsonValue(const Json::Value &j, ClockComponent &comp) {
        if (!j.isObject()) {
            return;
        }

        if (j.isMember("frequencyUnit")) {
            comp.frequencyUnit = (FrequencyUnit)j["frequencyUnit"].asInt();
        }

        if (j.isMember("frequency")) {
            comp.frequency = j["frequency"].asFloat();
        }

        if (j.isMember("dutyCycle")) {
            comp.dutyCycle = j.get("dutyCycle", comp.dutyCycle).asFloat();
        }
    }

    // --- DigitalComponent ---

    inline void toJsonValue(const DigitalComponent &comp, Json::Value &j) {
        j = Json::Value(Json::objectValue);
        j["type"] = static_cast<int>(comp.type);
        j["delay"] = comp.delay.count();

        // Serialize inputPins
        Json::Value &inputPinsArray = j["inputPins"] = Json::Value(Json::arrayValue);
        for (const auto &inputPinConnections : comp.inputPins) {
            Json::Value innerArray(Json::arrayValue);
            for (const auto &[id, idx] : inputPinConnections) {
                Json::Value connectionObj(Json::objectValue);
                connectionObj["id"] = static_cast<Json::UInt64>(id);
                connectionObj["index"] = idx;
                innerArray.append(connectionObj);
            }
            inputPinsArray.append(innerArray);
        }

        // Serialize outputPins
        Json::Value &outputPinsArray = j["outputPins"] = Json::Value(Json::arrayValue);
        for (const auto &outputPinConnections : comp.outputPins) {
            Json::Value innerArray(Json::arrayValue);
            for (const auto &[id, idx] : outputPinConnections) {
                Json::Value connectionObj(Json::objectValue);
                connectionObj["id"] = static_cast<Json::UInt64>(id);
                connectionObj["index"] = idx;
                innerArray.append(connectionObj);
            }
            outputPinsArray.append(innerArray);
        }

        // Serialize boolean vectors
        Json::Value &outputStatesArray = j["outputStates"] = Json::Value(Json::arrayValue);
        for (auto state : comp.outputStates) {
            outputStatesArray.append((bool)state);
        }

        Json::Value &inputStatesArray = j["inputStates"] = Json::Value(Json::arrayValue);
        for (auto state : comp.inputStates) {
            inputStatesArray.append((bool)state);
        }

        Json::Value &expressionsArray = j["expressions"] = Json::Value(Json::arrayValue);
        for (const auto &expr : comp.expressions) {
            expressionsArray.append(expr);
        }
    }

    inline void fromJsonValue(const Json::Value &j, DigitalComponent &comp) {
        if (!j.isObject()) {
            return;
        }

        comp.type = static_cast<ComponentType>(j.get("type", 0).asInt());
        comp.delay = SimDelayNanoSeconds(j.get("delay", 0).asInt64());

        // Deserialize inputPins
        if (j.isMember("inputPins")) {
            const Json::Value &inputPinsArray = j["inputPins"];
            comp.inputPins.clear();
            for (const auto &innerArray : inputPinsArray) {
                std::vector<std::pair<UUID, int>> inputVec;
                for (const auto &connectionObj : innerArray) {
                    UUID id = static_cast<UUID>(connectionObj.get("id", 0).asUInt64());
                    int index = connectionObj.get("index", 0).asInt();
                    inputVec.emplace_back(id, index);
                }
                comp.inputPins.push_back(inputVec);
            }
        }

        // Deserialize outputPins
        if (j.isMember("outputPins")) {
            const Json::Value &outputPinsArray = j["outputPins"];
            comp.outputPins.clear();
            for (const auto &innerArray : outputPinsArray) {
                std::vector<std::pair<UUID, int>> outputVec;
                for (const auto &connectionObj : innerArray) {
                    UUID id = static_cast<UUID>(connectionObj.get("id", 0).asUInt64());
                    int index = connectionObj.get("index", 0).asInt();
                    outputVec.emplace_back(id, index);
                }
                comp.outputPins.push_back(outputVec);
            }
        }

        // Deserialize boolean vectors
        if (j.isMember("outputStates")) {
            const Json::Value &outputStatesArray = j["outputStates"];
            comp.outputStates.clear();
            for (const auto &state : outputStatesArray) {
                comp.outputStates.push_back(state.asBool());
            }
        }

        if (j.isMember("inputStates")) {
            const Json::Value &inputStatesArray = j["inputStates"];
            comp.inputStates.clear();
            for (const auto &state : inputStatesArray) {
                comp.inputStates.push_back(state.asBool());
            }
        }

        if (j.isMember("expressions")) {
            const Json::Value &expressionsArr = j["expressions"];
            comp.expressions.clear();
            for (const auto &expr : expressionsArr) {
                comp.expressions.push_back(expr.asString());
            }
        } else {
            comp.expressions = ComponentCatalog::instance().getComponentDefinition(comp.type)->getExpressions(comp.inputPins.size());
        }
    }
} // namespace Bess::JsonConvert
