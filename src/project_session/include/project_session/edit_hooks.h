#pragma once

#include "common/bess_uuid.h"

#include <functional>
#include <memory>
#include <vector>

namespace Bess::Canvas {
    class Scene;
    class SceneComponent;
} // namespace Bess::Canvas

namespace Bess::SimEngine::Drivers {
    class CompDef;
}

namespace Bess::Edit {
    struct AddOpts {
        bool setZ = false;
        bool attach = true;
        bool event = true;
    };

    struct Hooks {
        using AddFn =
            std::function<bool(const std::shared_ptr<Canvas::Scene> &,
                               const std::shared_ptr<Canvas::SceneComponent> &,
                               const AddOpts &)>;
        using RmFn = std::function<std::vector<UUID>(
            const std::shared_ptr<Canvas::Scene> &,
            const std::shared_ptr<Canvas::SceneComponent> &,
            UUID)>;
        using DepsFn = std::function<std::vector<UUID>(
            const std::shared_ptr<Canvas::Scene> &,
            const std::shared_ptr<Canvas::SceneComponent> &)>;
        using SortFn = std::function<void(
            const std::shared_ptr<Canvas::Scene> &, std::vector<UUID> &)>;
        using MakeCompFn =
            std::function<std::vector<std::shared_ptr<Canvas::SceneComponent>>(
                const std::shared_ptr<SimEngine::Drivers::CompDef> &)>;

        AddFn add;
        RmFn rm;
        DepsFn deps;
        SortFn sort;
        MakeCompFn makeComp;
    };

    [[nodiscard]] const std::shared_ptr<const Hooks> &baseHooks();

    [[nodiscard]] bool
    add(const std::shared_ptr<Canvas::Scene> &scene,
        const std::shared_ptr<Canvas::SceneComponent> &comp,
        const AddOpts &opts,
        const std::shared_ptr<const Hooks> &hooks = baseHooks());

    [[nodiscard]] std::vector<UUID>
    rm(const std::shared_ptr<Canvas::Scene> &scene,
       const std::shared_ptr<Canvas::SceneComponent> &comp,
       UUID caller = UUID::master,
       const std::shared_ptr<const Hooks> &hooks = baseHooks());

    [[nodiscard]] std::vector<UUID>
    deps(const std::shared_ptr<Canvas::Scene> &scene,
         const std::shared_ptr<Canvas::SceneComponent> &comp,
         const std::shared_ptr<const Hooks> &hooks = baseHooks());

    void sort(const std::shared_ptr<Canvas::Scene> &scene,
              std::vector<UUID> &ids,
              const std::shared_ptr<const Hooks> &hooks = baseHooks());
} // namespace Bess::Edit
