#include "types.h"

namespace Bess::JsonConvert {
    using namespace Bess::SimEngine;
    void toJsonValue(const ComponentPin &pin, Json::Value &j) {
        j = Json::Value(Json::objectValue);
        toJsonValue(pin.first, j["componentUuid"]);
        j["pinIdx"] = pin.second;
    }

    void fromJsonValue(const Json::Value &j, ComponentPin &pin) {
        if (!j.isObject()) {
            return;
        }
        fromJsonValue(j["componentUuid"], pin.first);
        if (j.isMember("pinIdx")) {
            pin.second = j["pinIdx"].asInt();
        }
    }

    void toJsonValue(const Connections &connections, Json::Value &j) {
        j = Json::Value(Json::arrayValue);
        for (const auto &connList : connections) {
            Json::Value connArr = Json::Value(Json::arrayValue);
            for (const auto &conn : connList) {
                Json::Value connJ;
                toJsonValue(conn, connJ);
                connArr.append(connJ);
            }
            j.append(connArr);
        }
    }

    void fromJsonValue(const Json::Value &j, Connections &connections) {
        connections.clear();
        if (!j.isArray()) {
            return;
        }
        for (const auto &connArr : j) {
            std::vector<ComponentPin> connList;
            for (const auto &connJ : connArr) {
                ComponentPin conn;
                fromJsonValue(connJ, conn);
                connList.push_back(conn);
            }
            connections.push_back(connList);
        }
    }

    void toJsonValue(const SlotState &state, Json::Value &j) {
        j = Json::Value(Json::objectValue);
        j["state"] = static_cast<int>(state.state);
        j["lastChangeTime"] = static_cast<Json::Int64>(state.lastChangeTime.count());
    }

    void fromJsonValue(const Json::Value &j, SlotState &state) {
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
                SlotState pinState;
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
                SlotState pinState;
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
} // namespace Bess::JsonConvert
