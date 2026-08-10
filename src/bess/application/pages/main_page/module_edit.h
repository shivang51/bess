#pragma once

#include "common/bess_api.h"
#include "common/bess_uuid.h"
#include "project_session/project_transaction.h"

#include <memory>
#include <string>

namespace Bess {
    class ProjectSession;

    namespace Canvas {
        class ModuleSceneComponent;
        class Scene;
    } // namespace Canvas
} // namespace Bess

namespace Bess::Edit {
    [[nodiscard]] BESS_API
        ValResult<std::shared_ptr<Canvas::ModuleSceneComponent>>
        makeModule(ProjectSession &session,
                   const std::shared_ptr<Canvas::Scene> &scene,
                   UUID net,
                   std::string name = "New Module");

    [[nodiscard]] BESS_API Status rmModule(
        ProjectTx &tx, const std::shared_ptr<Canvas::Scene> &scene, UUID id);
} // namespace Bess::Edit
