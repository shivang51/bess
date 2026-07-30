#include "pages/main_page/main_page_edit_hooks.h"

#include "bess_core/scene/scene.h"
#include "bess_core/scene/scene_state/components/scene_component.h"
#include "bess_core/scene/scene_state/components/scene_component_types.h"
#include "pages/main_page/scene_components/connection_scene_component.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "pages/main_page/services/connection_service.h"
#include <algorithm>

namespace Bess::Pages {
    namespace {
        bool
        addComponent(const std::shared_ptr<Canvas::Scene> &scene,
                     const std::shared_ptr<Canvas::SceneComponent> &component,
                     const Edit::AddOpts &options,
                     const std::shared_ptr<Svc::SvcConnection> &service) {
            if (!scene || !component) {
                return false;
            }

            if (component->getType() ==
                Canvas::SceneComponentType::connection) {
                if (scene->getState().isComponentValid(component->getUuid())) {
                    return true;
                }

                const auto connection =
                    std::dynamic_pointer_cast<Canvas::ConnectionSceneComponent>(
                        component);
                return service && connection &&
                       service->addConnection(connection, scene);
            }

            return Edit::baseHooks()->add(scene, component, options);
        }

        std::vector<UUID> removeComponent(
            const std::shared_ptr<Canvas::Scene> &scene,
            const std::shared_ptr<Canvas::SceneComponent> &component,
            const UUID &callerId,
            const std::shared_ptr<Svc::SvcConnection> &service) {
            if (!scene || !component ||
                !scene->getState().isComponentValid(component->getUuid())) {
                return {};
            }

            if (component->getType() ==
                Canvas::SceneComponentType::connection) {
                const auto connection =
                    std::dynamic_pointer_cast<Canvas::ConnectionSceneComponent>(
                        component);
                if (service && connection) {
                    return service->removeConnection(connection, scene);
                }
            }

            return Edit::baseHooks()->rm(scene, component, callerId);
        }

        std::vector<UUID> getDependants(
            const std::shared_ptr<Canvas::Scene> &scene,
            const std::shared_ptr<Canvas::SceneComponent> &component) {
            return Edit::baseHooks()->deps(scene, component);
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

        std::vector<std::shared_ptr<Canvas::SceneComponent>>
        makeComponent(
            const std::shared_ptr<SimEngine::Drivers::CompDef> &def) {
            if (!def) {
                return {};
            }
            auto comps = Canvas::SimulationSceneComponent::createNew(def);
            if (comps.empty()) {
                return {};
            }
            const auto main = std::dynamic_pointer_cast<
                Canvas::SimulationSceneComponent>(comps.front());
            if (!main) {
                return {};
            }
            main->setCompDef(def->clone());
            return comps;
        }
    } // namespace

    std::shared_ptr<const Edit::Hooks>
    makeEditHooks(std::shared_ptr<Svc::SvcConnection> conn) {
        auto hooks = std::make_shared<Edit::Hooks>();
        hooks->add = [conn](const auto &scene,
                            const auto &comp,
                            const auto &opts) {
            return addComponent(scene, comp, opts, conn);
        };
        hooks->rm = [conn](const auto &scene,
                           const auto &comp,
                           UUID caller) {
            return removeComponent(scene, comp, caller, conn);
        };
        hooks->deps = getDependants;
        hooks->sort = sortDeletionOrder;
        hooks->makeComp = makeComponent;
        return hooks;
    }
} // namespace Bess::Pages
