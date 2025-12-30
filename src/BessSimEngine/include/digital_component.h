#pragma once

#include "bess_api.h"
#include "bess_uuid.h"
#include "component_definition.h"
#include "types.h"
#include <entt/entt.hpp>
#include <memory>

namespace Bess::SimEngine {

    struct BESS_API DigitalComponent {
        DigitalComponent() = default;
        DigitalComponent(const DigitalComponent &) = default;
        DigitalComponent(const std::shared_ptr<ComponentDefinition> &def);

        size_t incrementInputCount();
        size_t incrementOutputCount();

        UUID id;
        UUID netUuid = UUID::null;
        ComponentState state;
        std::shared_ptr<ComponentDefinition> definition;
        Connections inputConnections;
        Connections outputConnections;
    };

} // namespace Bess::SimEngine
