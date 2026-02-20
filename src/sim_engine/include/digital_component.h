#pragma once

#include "bess_api.h"
#include "common/bess_uuid.h"
#include "component_definition.h"
#include "types.h"
#include <memory>

namespace Bess::SimEngine {

    struct BESS_API DigitalComponent {
        DigitalComponent() = default;
        DigitalComponent(const DigitalComponent &) = default;
        DigitalComponent(const std::shared_ptr<ComponentDefinition> &def);

        size_t incrementInputCount();
        size_t incrementOutputCount();

        size_t decrementInputCount();
        size_t decrementOutputCount();

        UUID id;
        UUID netUuid = UUID::null;
        ComponentState state;
        std::shared_ptr<ComponentDefinition> definition;
        Connections inputConnections;
        Connections outputConnections;
    };

} // namespace Bess::SimEngine

namespace Bess::JsonConvert {
    BESS_API void toJsonValue(Json::Value &j, const Bess::SimEngine::DigitalComponent &comp);
    BESS_API void fromJsonValue(const Json::Value &j, Bess::SimEngine::DigitalComponent &comp);
} // namespace Bess::JsonConvert
