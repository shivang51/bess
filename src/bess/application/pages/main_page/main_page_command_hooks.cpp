#include "pages/main_page/main_page_command_hooks.h"

#include "bess_core/g_app_context.h"
#include "bess_core/project_context.h"
#include "bess_core/scene/scene.h"
#include "bess_core/scene/scene_state/components/scene_component.h"
#include "bess_core/scene/scene_state/components/scene_component_types.h"
#include "pages/main_page/scene_components/connection_scene_component.h"
#include "pages/main_page/services/connection_service.h"
#include <algorithm>

namespace Bess::Pages {
    namespace {
        std::shared_ptr<Svc::SvcConnection> getConnectionService() {
            const auto projectCtx =
                GAppContext::getInstance().getSubSystem<ProjectContext>();
            return projectCtx ? projectCtx->getSubSystem<Svc::SvcConnection>()
                              : nullptr;
        }

        bool
        addComponent(const std::shared_ptr<Canvas::Scene> &scene,
                     const std::shared_ptr<Canvas::SceneComponent> &component,
                     const Cmd::SceneComponentAddOptions &options) {
            if (!scene || !component) {
                return false;
            }

            if (component->getType() ==
                Canvas::SceneComponentType::connection) {
                if (scene->getState().isComponentValid(component->getUuid())) {
                    return true;
                }

                const auto service = getConnectionService();
                const auto connection =
                    std::dynamic_pointer_cast<Canvas::ConnectionSceneComponent>(
                        component);
                return service && connection &&
                       service->addConnection(connection, scene);
            }

            return Cmd::defaultSceneComponentCommandHooks()->addComponent(
                scene, component, options);
        }

        std::vector<UUID> removeComponent(
            const std::shared_ptr<Canvas::Scene> &scene,
            const std::shared_ptr<Canvas::SceneComponent> &component,
            const UUID &callerId) {
            if (!scene || !component ||
                !scene->getState().isComponentValid(component->getUuid())) {
                return {};
            }

            if (component->getType() ==
                Canvas::SceneComponentType::connection) {
                const auto service = getConnectionService();
                const auto connection =
                    std::dynamic_pointer_cast<Canvas::ConnectionSceneComponent>(
                        component);
                if (service && connection) {
                    return service->removeConnection(connection, scene);
                }
            }

            return Cmd::defaultSceneComponentCommandHooks()->removeComponent(
                scene, component, callerId);
        }

        std::vector<UUID> getDependants(
            const std::shared_ptr<Canvas::Scene> &scene,
            const std::shared_ptr<Canvas::SceneComponent> &component) {
            return Cmd::defaultSceneComponentCommandHooks()->getDependants(
                scene, component);
        }

        void sortDeletionOrder(const std::shared_ptr<Canvas::Scene> &scene,
                               std::vector<UUID> &componentIds) {
            if (!scene) {
                return;
            }

            const auto priority = [&scene](const UUID &componentId) {
                const auto component =
                    scene->getState().getComponentByUuid(componentId);
                if (!component) {
                    return 2;
                }

                return component->getType() ==
                               Canvas::SceneComponentType::connection
                           ? 0
                           : 1;
            };

            std::stable_sort(componentIds.begin(),
                             componentIds.end(),
                             [&](const UUID &lhs, const UUID &rhs) {
                                 return priority(lhs) < priority(rhs);
                             });
        }
    } // namespace

    std::shared_ptr<const Cmd::SceneComponentCommandHooks>
    createMainPageCommandHooks() {
        auto hooks = std::make_shared<Cmd::SceneComponentCommandHooks>();
        hooks->addComponent = addComponent;
        hooks->removeComponent = removeComponent;
        hooks->getDependants = getDependants;
        hooks->sortDeletionOrder = sortDeletionOrder;
        return hooks;
    }
} // namespace Bess::Pages
