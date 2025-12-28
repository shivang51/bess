#pragma once

#include "bess_api.h"
#include "bess_uuid.h"

namespace Bess::SimEngine::Events {
    struct BESS_API ComponentAddedEvent {
        UUID componentId;
    };

    struct BESS_API CompDefInputsResizedEvent {
        UUID componentId;
    };

    struct BESS_API CompDefOutputsResizedEvent {
        UUID componentId;
    };
} // namespace Bess::SimEngine::Events
