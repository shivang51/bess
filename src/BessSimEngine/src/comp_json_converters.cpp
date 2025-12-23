#include "comp_json_converters.h"

#include "component_catalog.h"

namespace Bess::JsonConvert {
    using namespace Bess::SimEngine;

    void toJsonValue(const IdComponent &comp, Json::Value &j) {
        j = Json::Value(Json::objectValue);
        j["uuid"] = static_cast<Json::UInt64>(comp.uuid);
    }

    void fromJsonValue(const Json::Value &j, IdComponent &comp) {
        if (j.isMember("uuid")) {
            comp.uuid = static_cast<UUID>(j["uuid"].asUInt64());
        }
    }

    void toJsonValue(const FlipFlopComponent &comp, Json::Value &j) {
        j = Json::Value(Json::objectValue);
        j["clockPinIdx"] = comp.clockPinIdx;
        j["prevClockState"] = static_cast<int>(comp.prevClockState);
    }

    void fromJsonValue(const Json::Value &j, FlipFlopComponent &comp) {
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

    void toJsonValue(const ClockComponent &comp, Json::Value &j) {
        j = Json::Value(Json::objectValue);
        j["frequency"] = comp.frequency;
        j["frequencyUnit"] = (int)comp.frequencyUnit;
        j["dutyCycle"] = comp.dutyCycle;
    }

    void fromJsonValue(const Json::Value &j, ClockComponent &comp) {
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

    void toJsonValue(const Bess::SimEngine::PinDetail &pin, Json::Value &j) {
        j = Json::Value(Json::objectValue);
        j["type"] = static_cast<int>(pin.type);
        j["name"] = pin.name;
        j["extendedType"] = static_cast<int>(pin.extendedType);
    }

    void fromJsonValue(const Json::Value &j, Bess::SimEngine::PinDetail &pin) {
        if (!j.isObject()) {
            return;
        }
        pin.type = static_cast<Bess::SimEngine::PinType>(j.get("type", 0).asInt());
        pin.name = j.get("name", pin.name).asString();
        pin.extendedType = static_cast<Bess::SimEngine::ExtendedPinType>(j.get("extendedType", -1).asInt());
    }

    void toJsonValue(const ComponentDefinition &def, Json::Value &j) {
        // j = Json::Value(Json::objectValue);
        // j["name"] = def.name;
        // j["category"] = def.category;
        // j["delay"] = static_cast<Json::Int64>(def.delay.count());
        // j["setupTime"] = static_cast<Json::Int64>(def.setupTime.count());
        // j["holdTime"] = static_cast<Json::Int64>(def.holdTime.count());
        // j["inputCount"] = def.inputCount;
        // j["outputCount"] = def.outputCount;
        // j["op"] = static_cast<int>(def.op);
        // j["negate"] = def.negate;
        //
        // // expressions
        // Json::Value &exprArr = j["expressions"] = Json::Value(Json::arrayValue);
        // for (const auto &expr : def.expressions) {
        //     exprArr.append(expr);
        // }
        //
        // // pin details
        // Json::Value &inPins = j["inputPinDetails"] = Json::Value(Json::arrayValue);
        // for (const auto &p : def.inputPinDetails) {
        //     Json::Value pj;
        //     toJsonValue(p, pj);
        //     inPins.append(pj);
        // }
        //
        // Json::Value &outPins = j["outputPinDetails"] = Json::Value(Json::arrayValue);
        // for (const auto &p : def.outputPinDetails) {
        //     Json::Value pj;
        //     toJsonValue(p, pj);
        //     outPins.append(pj);
        // }
        //
        // // stable identifier
        // j["hash"] = static_cast<Json::UInt64>(def.getHash());
    }

    void fromJsonValue(const Json::Value &j, ComponentDefinition &def) {
        // if (!j.isObject()) {
        //     return;
        // }
        // def.name = j.get("name", def.name).asString();
        // def.category = j.get("category", def.category).asString();
        // def.delay = SimDelayNanoSeconds(j.get("delay", 0).asInt64());
        // def.setupTime = SimDelayNanoSeconds(j.get("setupTime", 0).asInt64());
        // def.holdTime = SimDelayNanoSeconds(j.get("holdTime", 0).asInt64());
        // def.inputCount = j.get("inputCount", def.inputCount).asInt();
        // def.outputCount = j.get("outputCount", def.outputCount).asInt();
        // def.op = static_cast<char>(j.get("op", static_cast<int>(def.op)).asInt());
        // def.negate = j.get("negate", def.negate).asBool();
        // def.expressions.clear();
        // if (j.isMember("expressions")) {
        //     for (const auto &e : j["expressions"]) {
        //         def.expressions.push_back(e.asString());
        //     }
        // }
        //
        // def.inputPinDetails.clear();
        // if (j.isMember("inputPinDetails")) {
        //     for (const auto &pj : j["inputPinDetails"]) {
        //         PinDetail p{};
        //         fromJsonValue(pj, p);
        //         def.inputPinDetails.push_back(p);
        //     }
        // }
        //
        // def.outputPinDetails.clear();
        // if (j.isMember("outputPinDetails")) {
        //     for (const auto &pj : j["outputPinDetails"]) {
        //         PinDetail p{};
        //         fromJsonValue(pj, p);
        //         def.outputPinDetails.push_back(p);
        //     }
        // }
        //
        // auto hash = j.get("hash", 0).asUInt64();
        //
        // const auto &catalogDef = SimEngine::ComponentCatalog::instance().getComponentDefinition(hash);
        //
        // assert(catalogDef && "ComponentDefinition not found in catalog during deserialization");
        //
        // def.simulationFunction = catalogDef->simulationFunction;
        // def.auxData = catalogDef->auxData;
        //
        // def.reinit();
        //
        // assert(hash == def.getHash() && "Deserialized ComponentDefinition hash mismatch");
    }

    // struct BESS_API ComponentState {
    //     std::vector<PinState> inputStates;
    //     std::vector<bool> inputConnected;
    //     std::vector<PinState> outputStates;
    //     std::vector<bool> outputConnected;
    //     bool isChanged = false;
    //     std::any *auxData = nullptr;
    //     bool simError = false;
    //     std::string errorMessage;
    // };
    //

    void toJsonValue(const PinState &state, Json::Value &j) {
        j = Json::Value(Json::objectValue);
        j["state"] = static_cast<int>(state.state);
        j["lastChangeTime"] = static_cast<Json::Int64>(state.lastChangeTime.count());
    }

    void fromJsonValue(const Json::Value &j, PinState &state) {
        if (!j.isObject()) {
            return;
        }
        state.state = static_cast<LogicState>(j.get("state", 0).asInt());
        state.lastChangeTime = SimTime(j.get("lastChangeTime", 0).asInt64());
    }

    void toJsonValue(const ComponentState &state, Json::Value &j) {
        j = Json::Value(Json::objectValue);

        j["inputStates"] = Json::Value(Json::arrayValue);
        for (const auto &pinState : state.inputStates) {
            Json::Value pinJ;
            toJsonValue(pinState, pinJ);
            j["inputStates"].append(pinJ);
        }

        j["inputConnected"] = Json::Value(Json::arrayValue);
        for (const auto &connected : state.inputConnected) {
            j["inputConnected"].append(connected);
        }

        j["outputStates"] = Json::Value(Json::arrayValue);
        for (const auto &pinState : state.outputStates) {
            Json::Value pinJ;
            toJsonValue(pinState, pinJ);
            j["outputStates"].append(pinJ);
        }

        j["outputConnected"] = Json::Value(Json::arrayValue);
        for (const auto &connected : state.outputConnected) {
            j["outputConnected"].append(connected);
        }

        j["isChanged"] = state.isChanged;
        j["simError"] = state.simError;
        j["errorMessage"] = state.errorMessage;
    }

    void fromJsonValue(const Json::Value &j, ComponentState &state) {
        if (!j.isObject()) {
            return;
        }

        state.inputStates.clear();
        if (j.isMember("inputStates")) {
            for (const auto &pinJ : j["inputStates"]) {
                PinState pinState;
                fromJsonValue(pinJ, pinState);
                state.inputStates.push_back(pinState);
            }
        }

        state.inputConnected.clear();
        if (j.isMember("inputConnected")) {
            for (const auto &connJ : j["inputConnected"]) {
                state.inputConnected.push_back(connJ.asBool());
            }
        }

        state.outputStates.clear();
        if (j.isMember("outputStates")) {
            for (const auto &pinJ : j["outputStates"]) {
                PinState pinState;
                fromJsonValue(pinJ, pinState);
                state.outputStates.push_back(pinState);
            }
        }

        state.outputConnected.clear();
        if (j.isMember("outputConnected")) {
            for (const auto &connJ : j["outputConnected"]) {
                state.outputConnected.push_back(connJ.asBool());
            }
        }

        state.isChanged = j.get("isChanged", state.isChanged).asBool();
        state.simError = j.get("simError", state.simError).asBool();
        state.errorMessage = j.get("errorMessage", state.errorMessage).asString();
    }

    void toJsonValue(const ComponentPin &pin, Json::Value &j) {
        j = Json::Value(Json::objectValue);
        j["componentUuid"] = static_cast<Json::UInt64>(pin.first);
        j["pinIdx"] = pin.second;
    }

    void fromJsonValue(const Json::Value &j, ComponentPin &pin) {
        if (!j.isObject()) {
            return;
        }
        if (j.isMember("componentUuid")) {
            pin.first = static_cast<UUID>(j["componentUuid"].asUInt64());
        }
        if (j.isMember("pinIdx")) {
            pin.second = j["pinIdx"].asInt();
        }
    }

    void toJsonValue(const DigitalComponent &comp, Json::Value &j) {
        j = Json::Value(Json::objectValue);

        toJsonValue(comp.definition, j["definition"]);
        toJsonValue(comp.state, j["state"]);
        j["inputConnections"] = Json::Value(Json::arrayValue);
        for (const auto &connList : comp.inputConnections) {
            Json::Value connArr = Json::Value(Json::arrayValue);
            for (const auto &conn : connList) {
                Json::Value connJ;
                toJsonValue(conn, connJ);
                connArr.append(connJ);
            }
            j["inputConnections"].append(connArr);
        }

        j["outputConnections"] = Json::Value(Json::arrayValue);
        for (const auto &connList : comp.outputConnections) {
            Json::Value connArr = Json::Value(Json::arrayValue);
            for (const auto &conn : connList) {
                Json::Value connJ;
                toJsonValue(conn, connJ);
                connArr.append(connJ);
            }
            j["outputConnections"].append(connArr);
        }
    }

    void fromJsonValue(const Json::Value &j, DigitalComponent &comp) {
        if (!j.isObject()) {
            return;
        }

        fromJsonValue(j["definition"], comp.definition);
        fromJsonValue(j["state"], comp.state);
        // comp.state.auxData = &comp.definition.auxData;

        if (j.isMember("inputConnections")) {
            for (const auto &connArr : j["inputConnections"]) {
                std::vector<ComponentPin> connList;
                for (const auto &connJ : connArr) {
                    ComponentPin conn;
                    fromJsonValue(connJ, conn);
                    connList.push_back(conn);
                }
                comp.inputConnections.push_back(connList);
            }
        }

        comp.outputConnections.clear();
        if (j.isMember("outputConnections")) {
            for (const auto &connArr : j["outputConnections"]) {
                std::vector<ComponentPin> connList;
                for (const auto &connJ : connArr) {
                    ComponentPin conn;
                    fromJsonValue(connJ, conn);
                    connList.push_back(conn);
                }
                comp.outputConnections.push_back(connList);
            }
        }
    }
} // namespace Bess::JsonConvert
