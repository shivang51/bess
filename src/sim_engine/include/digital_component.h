#pragma once

#include "bess_api.h"
#include "common/bess_uuid.h"
#include "component_definition.h"
#include "types.h"
#include <memory>

namespace Bess::SimEngine {

    typedef std::function<void(ComponentState &, ComponentState &)> TOnStateChangeCB;

    struct BESS_API DigitalComponent {
        DigitalComponent() = default;
        DigitalComponent(const DigitalComponent &) = default;
        DigitalComponent(const std::shared_ptr<ComponentDefinition> &def);

        size_t incrementInputCount();
        size_t incrementOutputCount();

        size_t decrementInputCount();
        size_t decrementOutputCount();

        void addOnStateChangeCB(const TOnStateChangeCB &cb);

        void dispatchStateChange(ComponentState &oldState, ComponentState &newState);

        UUID id;
        UUID netUuid = UUID::null;
        ComponentState state;
        std::shared_ptr<ComponentDefinition> definition;
        Connections inputConnections;
        Connections outputConnections;

        std::vector<TOnStateChangeCB> onStateChangeCbs;
    };

} // namespace Bess::SimEngine

namespace Bess::JsonConvert {
    BESS_API void toJsonValue(Json::Value &j, const Bess::SimEngine::DigitalComponent &comp);
    BESS_API void fromJsonValue(const Json::Value &j, Bess::SimEngine::DigitalComponent &comp);
} // namespace Bess::JsonConvert
