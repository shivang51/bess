#pragma once

#include "common/bess_api.h"
#include "common/bess_uuid.h"
#include <functional>
#include <memory>
#include <vector>

namespace Bess::Canvas {
    class Scene;
    class SceneComponent;
} // namespace Bess::Canvas

namespace Bess::Cmd {
    struct SceneComponentAddOptions {
        bool setZ = false;
        bool triggerAttach = true;
        bool dispatchEvent = true;
    };

    struct BESS_API SceneComponentCommandHooks {
        using AddComponentFn =
            std::function<bool(const std::shared_ptr<Canvas::Scene> &,
                               const std::shared_ptr<Canvas::SceneComponent> &,
                               const SceneComponentAddOptions &)>;

        using RemoveComponentFn = std::function<std::vector<UUID>(
            const std::shared_ptr<Canvas::Scene> &,
            const std::shared_ptr<Canvas::SceneComponent> &,
            const UUID &)>;

        using GetDependantsFn = std::function<std::vector<UUID>(
            const std::shared_ptr<Canvas::Scene> &,
            const std::shared_ptr<Canvas::SceneComponent> &)>;

        using SortDeletionOrderFn = std::function<void(
            const std::shared_ptr<Canvas::Scene> &, std::vector<UUID> &)>;

        AddComponentFn addComponent;
        RemoveComponentFn removeComponent;
        GetDependantsFn getDependants;
        SortDeletionOrderFn sortDeletionOrder;
    };

    BESS_API const std::shared_ptr<const SceneComponentCommandHooks> &
    defaultSceneComponentCommandHooks();

    BESS_API bool addSceneComponentWithHooks(
        const std::shared_ptr<Canvas::Scene> &scene,
        const std::shared_ptr<Canvas::SceneComponent> &component,
        const SceneComponentAddOptions &options,
        const std::shared_ptr<const SceneComponentCommandHooks> &hooks =
            defaultSceneComponentCommandHooks());

    BESS_API std::vector<UUID> removeSceneComponentWithHooks(
        const std::shared_ptr<Canvas::Scene> &scene,
        const std::shared_ptr<Canvas::SceneComponent> &component,
        const UUID &callerId,
        const std::shared_ptr<const SceneComponentCommandHooks> &hooks =
            defaultSceneComponentCommandHooks());

    BESS_API std::vector<UUID> getSceneComponentDependantsWithHooks(
        const std::shared_ptr<Canvas::Scene> &scene,
        const std::shared_ptr<Canvas::SceneComponent> &component,
        const std::shared_ptr<const SceneComponentCommandHooks> &hooks =
            defaultSceneComponentCommandHooks());

    BESS_API void sortSceneComponentDeletionOrderWithHooks(
        const std::shared_ptr<Canvas::Scene> &scene,
        std::vector<UUID> &componentIds,
        const std::shared_ptr<const SceneComponentCommandHooks> &hooks =
            defaultSceneComponentCommandHooks());
} // namespace Bess::Cmd
