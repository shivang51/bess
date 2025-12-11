#pragma once
#include "bess_api.h"
#include "bess_uuid.h"
#include "entt_components.h"
#include "types.h"
#include "json/json.h"

namespace Bess::JsonConvert {
    using namespace Bess::SimEngine;

    // --- IdComponent ---
    void toJsonValue(const IdComponent &comp, Json::Value &j);
    void fromJsonValue(const Json::Value &j, IdComponent &comp);

    // --- FlipFlopComonent ---
    void toJsonValue(const FlipFlopComponent &comp, Json::Value &j);
    void fromJsonValue(const Json::Value &j, FlipFlopComponent &comp);

    // --- ClockComponent ---
    void toJsonValue(const ClockComponent &comp, Json::Value &j);
    void fromJsonValue(const Json::Value &j, ClockComponent &comp);

    // --- PinDetails ---
    void toJsonValue(const Bess::SimEngine::PinDetail &pin, Json::Value &j);
    void fromJsonValue(const Json::Value &j, Bess::SimEngine::PinDetail &pin);

    // --- ComponentDefinition ---
    void toJsonValue(const ComponentDefinition &def, Json::Value &j);
    void fromJsonValue(const Json::Value &j, ComponentDefinition &def);

    // --- PinState ---
    void toJsonValue(const PinState &state, Json::Value &j);
    void fromJsonValue(const Json::Value &j, PinState &state);

    // --- ComponentState ---
    void toJsonValue(const ComponentState &state, Json::Value &j);
    void fromJsonValue(const Json::Value &j, ComponentState &state);

    // --- DigitalComponent ---
    void toJsonValue(const DigitalComponent &comp, Json::Value &j);
    void fromJsonValue(const Json::Value &j, DigitalComponent &comp);

} // namespace Bess::JsonConvert
