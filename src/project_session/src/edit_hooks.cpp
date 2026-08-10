#include "project_session/edit_hooks.h"

#include "bess_core/scene/scene.h"
#include "bess_core/scene/scene_state/components/scene_component.h"
#include "bess_core/scene/scene_state/scene_state.h"

namespace Bess::Edit {
    namespace {
        bool baseAdd(const std::shared_ptr<Canvas::Scene> &scene,
                     const std::shared_ptr<Canvas::SceneComponent> &comp,
                     const AddOpts &opts) {
            if (!scene || !comp) {
                return false;
            }

            auto &state = scene->getState();
            if (state.isComponentValid(comp->getUuid())) {
                return true;
            }

            if (opts.setZ) {
                auto pos = comp->getTransform().position;
                pos.z = scene->getNextZCoord();
                comp->setPosition(pos);
            }

            state.addComponent(comp, opts.attach, opts.event);
            return state.isComponentValid(comp->getUuid());
        }

        std::vector<UUID>
        baseRm(const std::shared_ptr<Canvas::Scene> &scene,
               const std::shared_ptr<Canvas::SceneComponent> &comp,
               UUID caller) {
            if (!scene || !comp ||
                !scene->getState().isComponentValid(comp->getUuid())) {
                return {};
            }
            return scene->getState().removeComponent(comp->getUuid(), caller);
        }

        std::vector<UUID>
        baseDeps(const std::shared_ptr<Canvas::Scene> &scene,
                 const std::shared_ptr<Canvas::SceneComponent> &comp) {
            if (!scene || !comp) {
                return {};
            }
            return comp->getDependants(scene->getState());
        }

        void baseSort(const std::shared_ptr<Canvas::Scene> &,
                      std::vector<UUID> &) {
        }

        std::shared_ptr<const Hooks> makeBase() {
            auto hooks = std::make_shared<Hooks>();
            hooks->add = baseAdd;
            hooks->rm = baseRm;
            hooks->deps = baseDeps;
            hooks->sort = baseSort;
            return hooks;
        }
    } // namespace

    const std::shared_ptr<const Hooks> &baseHooks() {
        static const auto hooks = makeBase();
        return hooks;
    }

    bool add(const std::shared_ptr<Canvas::Scene> &scene,
             const std::shared_ptr<Canvas::SceneComponent> &comp,
             const AddOpts &opts,
             const std::shared_ptr<const Hooks> &hooks) {
        const auto &active = hooks ? hooks : baseHooks();
        return active->add ? active->add(scene, comp, opts)
                           : baseAdd(scene, comp, opts);
    }

    std::vector<UUID> rm(const std::shared_ptr<Canvas::Scene> &scene,
                         const std::shared_ptr<Canvas::SceneComponent> &comp,
                         UUID caller,
                         const std::shared_ptr<const Hooks> &hooks) {
        const auto &active = hooks ? hooks : baseHooks();
        return active->rm ? active->rm(scene, comp, caller)
                          : baseRm(scene, comp, caller);
    }

    std::vector<UUID> deps(const std::shared_ptr<Canvas::Scene> &scene,
                           const std::shared_ptr<Canvas::SceneComponent> &comp,
                           const std::shared_ptr<const Hooks> &hooks) {
        const auto &active = hooks ? hooks : baseHooks();
        return active->deps ? active->deps(scene, comp) : baseDeps(scene, comp);
    }

    void sort(const std::shared_ptr<Canvas::Scene> &scene,
              std::vector<UUID> &ids,
              const std::shared_ptr<const Hooks> &hooks) {
        const auto &active = hooks ? hooks : baseHooks();
        if (active->sort) {
            active->sort(scene, ids);
        } else {
            baseSort(scene, ids);
        }
    }
} // namespace Bess::Edit
