#pragma once

#include "bess_core/scene/scene_state/components/scene_component.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include "common/logger.h"

#include <string>

namespace Bess::Edit {
    inline bool trackComp(Canvas::SceneComponent &comp,
                          Json::Value before,
                          std::string key = {}) {
        auto *state = comp.sceneState();
        if (!state) {
            comp.applyJson(before);
            BESS_WARN("Could not track component edit: scene is unavailable");
            return false;
        }

        return state->trackComp(comp, std::move(before), std::move(key));
    }
} // namespace Bess::Edit
