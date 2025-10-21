#pragma once
#include "bess_api.h"
#include "bess_uuid.h"
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
        j["clockPinIdx"] = comp.clockPinIdx;
        j["prevClockState"] = static_cast<int>(comp.prevClockState);
    }

    inline void fromJsonValue(const Json::Value &j, FlipFlopComponent &comp) {
        if (!j.isObject()) {
            return;
        }
        if (j.isMember("clockPinIdx")) {
            comp.clockPinIdx = j["clockPinIdx"].asInt();
        }
        if (j.isMember("prevClockState")) {
            comp.prevClockState = static_cast<LogicState>(j["prevClockState"].asInt());
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
        // j["type"] = static_cast<int>(comp.type);
        // j["delay"] = comp.delay.count();

        // Serialize inputPins
        Json::Value &inputPinsArray = j["inputPins"] = Json::Value(Json::arrayValue);
        for (const auto &inputPinConnections : comp.inputConnections) {
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
        for (const auto &outputPinConnections : comp.outputConnections) {
            Json::Value innerArray(Json::arrayValue);
            for (const auto &[id, idx] : outputPinConnections) {
                Json::Value connectionObj(Json::objectValue);
                connectionObj["id"] = static_cast<Json::UInt64>(id);
                connectionObj["index"] = idx;
                innerArray.append(connectionObj);
            }
            outputPinsArray.append(innerArray);
        }

        // // Serialize boolean vectors
        // Json::Value &outputStatesArray = j["outputStates"] = Json::Value(Json::arrayValue);
        // for (auto state : comp.outputStates) {
        //     outputStatesArray.append((bool)state);
        // }
        //
        // Json::Value &inputStatesArray = j["inputStates"] = Json::Value(Json::arrayValue);
        // for (auto state : comp.inputStates) {
        //     inputStatesArray.append((bool)state);
        // }
        //
        // Json::Value &expressionsArray = j["expressions"] = Json::Value(Json::arrayValue);
        // for (const auto &expr : comp.expressions) {
        //     expressionsArray.append(expr);
        // }
    }

    inline void fromJsonValue(const Json::Value &j, DigitalComponent &comp) {
        if (!j.isObject()) {
            return;
        }

        // comp.type = static_cast<ComponentType>(j.get("type", 0).asInt());
        // comp.delay = SimDelayNanoSeconds(j.get("delay", 0).asInt64());

        // Deserialize inputPins
        if (j.isMember("inputPins")) {
            const Json::Value &inputPinsArray = j["inputPins"];
            comp.inputConnections.clear();
            for (const auto &innerArray : inputPinsArray) {
                std::vector<std::pair<UUID, int>> inputVec;
                for (const auto &connectionObj : innerArray) {
                    UUID id = static_cast<UUID>(connectionObj.get("id", 0).asUInt64());
                    int index = connectionObj.get("index", 0).asInt();
                    inputVec.emplace_back(id, index);
                }
                comp.inputConnections.push_back(inputVec);
            }
        }

        // Deserialize outputPins
        if (j.isMember("outputPins")) {
            const Json::Value &outputPinsArray = j["outputPins"];
            comp.outputConnections.clear();
            for (const auto &innerArray : outputPinsArray) {
                std::vector<std::pair<UUID, int>> outputVec;
                for (const auto &connectionObj : innerArray) {
                    UUID id = static_cast<UUID>(connectionObj.get("id", 0).asUInt64());
                    int index = connectionObj.get("index", 0).asInt();
                    outputVec.emplace_back(id, index);
                }
                comp.outputConnections.push_back(outputVec);
            }
        }

        // Deserialize boolean vectors
        // if (j.isMember("outputStates")) {
        //     const Json::Value &outputStatesArray = j["outputStates"];
        //     comp.outputStates.clear();
        //     for (const auto &state : outputStatesArray) {
        //         comp.outputStates.push_back(state.asBool());
        //     }
        // }
        //
        // if (j.isMember("inputStates")) {
        //     const Json::Value &inputStatesArray = j["inputStates"];
        //     comp.inputStates.clear();
        //     for (const auto &state : inputStatesArray) {
        //         comp.inputStates.push_back(state.asBool());
        //     }
        // }

        // if (j.isMember("expressions")) {
        //     const Json::Value &expressionsArr = j["expressions"];
        //     comp.expressions.clear();
        //     for (const auto &expr : expressionsArr) {
        //         comp.expressions.push_back(expr.asString());
        //     }
        // } else {
        //     comp.expressions = ComponentCatalog::instance().getComponentDefinition(comp.type)->getExpressions(comp.inputConnections.size());
        // }
    }

    // --- PinDetails ---

    inline void toJsonValue(const Bess::SimEngine::PinDetail &pin, Json::Value &j) {
        j = Json::Value(Json::objectValue);
        j["type"] = static_cast<int>(pin.type);
        j["name"] = pin.name;
        j["extendedType"] = static_cast<int>(pin.extendedType);
    }

    inline void fromJsonValue(const Json::Value &j, Bess::SimEngine::PinDetail &pin) {
        if (!j.isObject()) {
            return;
        }
        pin.type = static_cast<Bess::SimEngine::PinType>(j.get("type", 0).asInt());
        pin.name = j.get("name", pin.name).asString();
        pin.extendedType = static_cast<Bess::SimEngine::ExtendedPinType>(j.get("extendedType", -1).asInt());
    }

    // --- ComponentDefinition ---

    inline void toJsonValue(const ComponentDefinition &def, Json::Value &j) {
        j = Json::Value(Json::objectValue);
        j["name"] = def.name;
        j["category"] = def.category;
        j["delay"] = static_cast<Json::Int64>(def.delay.count());
        j["setupTime"] = static_cast<Json::Int64>(def.setupTime.count());
        j["holdTime"] = static_cast<Json::Int64>(def.holdTime.count());
        j["inputCount"] = def.inputCount;
        j["outputCount"] = def.outputCount;
        j["op"] = static_cast<int>(def.op);
        j["negate"] = def.negate;

        // expressions
        Json::Value &exprArr = j["expressions"] = Json::Value(Json::arrayValue);
        for (const auto &expr : def.expressions) {
            exprArr.append(expr);
        }

        // pin details
        Json::Value &inPins = j["inputPinDetails"] = Json::Value(Json::arrayValue);
        for (const auto &p : def.inputPinDetails) {
            Json::Value pj;
            toJsonValue(p, pj);
            inPins.append(pj);
        }

        Json::Value &outPins = j["outputPinDetails"] = Json::Value(Json::arrayValue);
        for (const auto &p : def.outputPinDetails) {
            Json::Value pj;
            toJsonValue(p, pj);
            outPins.append(pj);
        }

        // auxData is runtime-only; skip serializing to avoid non-determinism

        // stable identifier
        j["hash"] = static_cast<Json::UInt64>(def.getHash());
    }

    inline void fromJsonValue(const Json::Value &j, ComponentDefinition &def) {
        if (!j.isObject()) {
            return;
        }
        def.name = j.get("name", def.name).asString();
        def.category = j.get("category", def.category).asString();
        def.delay = SimDelayNanoSeconds(j.get("delay", 0).asInt64());
        def.setupTime = SimDelayNanoSeconds(j.get("setupTime", 0).asInt64());
        def.holdTime = SimDelayNanoSeconds(j.get("holdTime", 0).asInt64());
        def.inputCount = j.get("inputCount", def.inputCount).asInt();
        def.outputCount = j.get("outputCount", def.outputCount).asInt();
        def.op = static_cast<char>(j.get("op", static_cast<int>(def.op)).asInt());
        def.negate = j.get("negate", def.negate).asBool();

        def.expressions.clear();
        if (j.isMember("expressions")) {
            for (const auto &e : j["expressions"]) {
                def.expressions.push_back(e.asString());
            }
        }

        def.inputPinDetails.clear();
        if (j.isMember("inputPinDetails")) {
            for (const auto &pj : j["inputPinDetails"]) {
                PinDetail p{};
                fromJsonValue(pj, p);
                def.inputPinDetails.push_back(p);
            }
        }

        def.outputPinDetails.clear();
        if (j.isMember("outputPinDetails")) {
            for (const auto &pj : j["outputPinDetails"]) {
                PinDetail p{};
                fromJsonValue(pj, p);
                def.outputPinDetails.push_back(p);
            }
        }

        // auxData is runtime-only; do not deserialize

        // Note: simulationFunction and modifiable properties are not deserialized here due to runtime-only nature
    }
} // namespace Bess::JsonConvert
