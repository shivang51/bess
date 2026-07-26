#pragma once

#include "common/bess_api.h"
#include "common/bess_uuid.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Bess::Session {
    enum class ChangeKind : uint8_t {
        projectReset,
        sceneAdded,
        sceneRemoved,
        activeSceneChanged,
        entityAdded,
        entityRemoved,
        entityChanged,
        hierarchyChanged,
        simulationStateChanged,
        transactionUndone,
        transactionRedone,
        saved,
    };

    struct BESS_API SessionChange {
        ChangeKind kind = ChangeKind::entityChanged;
        UUID scene = UUID::null;
        UUID entity = UUID::null;
        std::string detail;
    };

    using SessionChangeSet = std::vector<SessionChange>;
} // namespace Bess::Session
