#include "bess_core/commands/scene_component_command_hooks.h"

#include "bess_core/scene/scene.h"
#include "bess_core/scene/scene_state/components/scene_component.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include "common/logger.h"

namespace Bess::Cmd {
    namespace {
        bool defaultAddComponent(
            const std::shared_ptr<Canvas::Scene> &scene,
            const std::shared_ptr<Canvas::SceneComponent> &component,
            const SceneComponentAddOptions &options) {
            if (!scene || !component) {
                return false;
            }

            if (scene->getState().isComponentValid(component->getUuid())) {
                return true;
            }

            if (options.setZ) {
                auto position = component->getTransform().position;
                position.z = scene->getNextZCoord();
                component->setPosition(position);
            }

            scene->getState().addComponent(
                component, options.triggerAttach, options.dispatchEvent);
            return scene->getState().isComponentValid(component->getUuid());
        }

        std::vector<UUID> defaultRemoveComponent(
            const std::shared_ptr<Canvas::Scene> &scene,
            const std::shared_ptr<Canvas::SceneComponent> &component,
            const UUID &callerId) {
            if (!scene || !component ||
                !scene->getState().isComponentValid(component->getUuid())) {
                return {};
            }

            return scene->getState().removeComponent(component->getUuid(),
                                                     callerId);
        }

        std::vector<UUID> defaultGetDependants(
            const std::shared_ptr<Canvas::Scene> &scene,
            const std::shared_ptr<Canvas::SceneComponent> &component) {
            if (!scene || !component) {
                return {};
            }

            return component->getDependants(scene->getState());
        }

        void
        defaultSortDeletionOrder(const std::shared_ptr<Canvas::Scene> &scene,
                                 std::vector<UUID> &componentIds) {
            (void)scene;
            (void)componentIds;
        }

        std::shared_ptr<const SceneComponentCommandHooks> makeDefaultHooks() {
            auto hooks = std::make_shared<SceneComponentCommandHooks>();
            hooks->addComponent = defaultAddComponent;
            hooks->removeComponent = defaultRemoveComponent;
            hooks->getDependants = defaultGetDependants;
            hooks->sortDeletionOrder = defaultSortDeletionOrder;
            return hooks;
        }
    } // namespace

    const std::shared_ptr<const SceneComponentCommandHooks> &
    defaultSceneComponentCommandHooks() {
        static const auto hooks = makeDefaultHooks();
        return hooks;
    }

    bool addSceneComponentWithHooks(
        const std::shared_ptr<Canvas::Scene> &scene,
        const std::shared_ptr<Canvas::SceneComponent> &component,
        const SceneComponentAddOptions &options,
        const std::shared_ptr<const SceneComponentCommandHooks> &hooks) {
        const auto activeHooks =
            hooks ? hooks : defaultSceneComponentCommandHooks();
        if (!activeHooks->addComponent) {
            return defaultAddComponent(scene, component, options);
        }

        return activeHooks->addComponent(scene, component, options);
    }

    std::vector<UUID> removeSceneComponentWithHooks(
        const std::shared_ptr<Canvas::Scene> &scene,
        const std::shared_ptr<Canvas::SceneComponent> &component,
        const UUID &callerId,
        const std::shared_ptr<const SceneComponentCommandHooks> &hooks) {
        const auto activeHooks =
            hooks ? hooks : defaultSceneComponentCommandHooks();
        if (!activeHooks->removeComponent) {
            return defaultRemoveComponent(scene, component, callerId);
        }

        return activeHooks->removeComponent(scene, component, callerId);
    }

    std::vector<UUID> getSceneComponentDependantsWithHooks(
        const std::shared_ptr<Canvas::Scene> &scene,
        const std::shared_ptr<Canvas::SceneComponent> &component,
        const std::shared_ptr<const SceneComponentCommandHooks> &hooks) {
        const auto activeHooks =
            hooks ? hooks : defaultSceneComponentCommandHooks();
        if (!activeHooks->getDependants) {
            return defaultGetDependants(scene, component);
        }

        return activeHooks->getDependants(scene, component);
    }

    void sortSceneComponentDeletionOrderWithHooks(
        const std::shared_ptr<Canvas::Scene> &scene,
        std::vector<UUID> &componentIds,
        const std::shared_ptr<const SceneComponentCommandHooks> &hooks) {
        const auto activeHooks =
            hooks ? hooks : defaultSceneComponentCommandHooks();
        if (!activeHooks->sortDeletionOrder) {
            defaultSortDeletionOrder(scene, componentIds);
            return;
        }

        activeHooks->sortDeletionOrder(scene, componentIds);
    }
} // namespace Bess::Cmd
