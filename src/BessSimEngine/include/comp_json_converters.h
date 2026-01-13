#pragma once
#include "bess_api.h"
#include "bess_uuid.h"
#include "digital_component.h"
#include "types.h"
#include "json/json.h"

namespace Bess::JsonConvert {
    using namespace Bess::SimEngine;

    // // --- ComponentDefinition ---
    // void toJsonValue(const ComponentDefinition &def, Json::Value &j);
    // void fromJsonValue(const Json::Value &j, ComponentDefinition &def);
    //
    // // --- PinState ---
    // void toJsonValue(const SlotState &state, Json::Value &j);
    // void fromJsonValue(const Json::Value &j, SlotState &state);
    //
    // // --- ComponentState ---
    // void toJsonValue(const ComponentState &state, Json::Value &j);
    // void fromJsonValue(const Json::Value &j, ComponentState &state);
    //
    // // --- DigitalComponent ---
    // void toJsonValue(const DigitalComponent &comp, Json::Value &j);
    // void fromJsonValue(const Json::Value &j, DigitalComponent &comp);

} // namespace Bess::JsonConvert
