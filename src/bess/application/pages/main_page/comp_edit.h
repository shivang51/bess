#pragma once

#include "bess_core/g_app_context.h"
#include "bess_core/scene/scene_state/components/scene_component.h"
#include "common/logger.h"
#include "project_session/project_session.h"

#include <string>

namespace Bess::Edit {
    inline bool trackComp(Canvas::SceneComponent &comp,
                          UUID scene,
                          Json::Value before,
                          std::string key = {}) {
        const auto after = comp.toJson();
        if (before == after) {
            return true;
        }

        const auto session =
            GAppContext::getInstance().getSubSystem<ProjectSession>();
        if (!session) {
            comp.applyJson(before);
            BESS_WARN("Could not track component edit: session is unavailable");
            return false;
        }

        const auto result = session->trackComp(comp.getUuid(),
                                               before,
                                               after,
                                               scene,
                                               std::move(key));
        if (result) {
            return true;
        }

        comp.applyJson(before);
        BESS_WARN("Could not track component edit: {}", result.status.msg());
        return false;
    }
} // namespace Bess::Edit
