#include "pages/main_page/module_edit.h"

#include "bess_core/g_app_context.h"
#include "bess_core/scene/scene.h"
#include "bess_core/scene_driver.h"
#include "dig_module_def.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/scene_components/connection_scene_component.h"
#include "pages/main_page/scene_components/module_scene_component.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "project_session/project_session.h"

#include <functional>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Bess::Edit {
    namespace {
        void moveComps(
            Canvas::Scene &from,
            Canvas::Scene &to,
            const std::vector<std::shared_ptr<Canvas::SceneComponent>> &comps) {
            auto &fromState = from.getState();
            auto &toState = to.getState();
            for (const auto &comp : comps) {
                if (!comp) {
                    continue;
                }
                fromState.removeFromMap(comp->getUuid());
                toState.addComponent(comp, false, false);
            }
        }

        std::vector<std::shared_ptr<Canvas::SceneComponent>>
        collect(const std::shared_ptr<Canvas::Scene> &scene,
                const std::vector<UUID> &ids) {
            std::unordered_set<UUID> seen;
            std::vector<std::shared_ptr<Canvas::SceneComponent>> comps;
            const auto add = [&](UUID id, const auto &self) -> void {
                if (seen.contains(id)) {
                    return;
                }
                const auto comp =
                    scene->getState()
                        .getComponentByUuidSP<Canvas::SceneComponent>(id);
                if (!comp) {
                    return;
                }
                seen.insert(id);
                for (const auto dep : comp->getDependants(scene->getState())) {
                    self(dep, self);
                }
                comps.push_back(comp);
            };
            for (const auto id : ids) {
                add(id, add);
            }
            return comps;
        }

        std::vector<UUID> roots(const std::shared_ptr<Canvas::Scene> &scene) {
            const auto &ids = scene->getState().getRootComponents();
            return {ids.begin(), ids.end()};
        }

        std::vector<std::shared_ptr<Canvas::SceneComponent>>
        kids(const std::shared_ptr<Canvas::Scene> &scene,
             const std::shared_ptr<Canvas::SceneComponent> &comp) {
            std::vector<std::shared_ptr<Canvas::SceneComponent>> out;
            if (!scene || !comp) {
                return out;
            }
            for (const auto id : comp->getChildComponents()) {
                const auto child =
                    scene->getState()
                        .getComponentByUuidSP<Canvas::SceneComponent>(id);
                if (child) {
                    out.push_back(child);
                }
            }
            return out;
        }

        std::shared_ptr<Canvas::ModuleSceneComponent>
        owner(const std::shared_ptr<Canvas::Scene> &scene,
              ProjectSession &session) {
            const auto &state = scene->getState();
            if (state.getIsRootScene() || state.getModuleId() == UUID::null ||
                state.getParentSceneId() == UUID::null) {
                return nullptr;
            }
            const auto parent =
                session.scenes().getSceneWithId(state.getParentSceneId());
            return parent ? parent->getState()
                                .getComponentByUuidSP<
                                    Canvas::ModuleSceneComponent>(
                                    state.getModuleId())
                          : nullptr;
        }

        std::unordered_set<UUID>
        connIds(const std::shared_ptr<Canvas::Scene> &scene, UUID id) {
            std::unordered_set<UUID> ids;
            const auto comp =
                scene->getState()
                    .getComponentByUuidSP<Canvas::SimulationSceneComponent>(id);
            if (!comp) {
                return ids;
            }
            const auto scan = [&](const std::vector<UUID> &slots) {
                for (const auto slotId : slots) {
                    const auto slot =
                        scene->getState()
                            .getComponentByUuidSP<Canvas::SlotSceneComponent>(
                                slotId);
                    if (!slot || slot->isResizeSlot()) {
                        continue;
                    }
                    ids.insert(slot->getConnectedConnections().begin(),
                               slot->getConnectedConnections().end());
                }
            };
            scan(comp->getInputSlots());
            scan(comp->getOutputSlots());
            return ids;
        }

        std::unordered_set<UUID>
        boundarySkip(const std::shared_ptr<Canvas::Scene> &scene,
                     ProjectSession &session,
                     UUID net,
                     std::vector<UUID> &rmConns) {
            std::unordered_set<UUID> skip;
            const auto mod = owner(scene, session);
            if (!mod) {
                return skip;
            }

            for (const auto id :
                 {mod->getAssociatedInp(), mod->getAssociatedOut()}) {
                const auto comp =
                    scene->getState()
                        .getComponentByUuidSP<Canvas::SimulationSceneComponent>(
                            id);
                if (!comp || comp->getNetId() != net) {
                    continue;
                }
                for (const auto &one : collect(scene, {id})) {
                    if (one) {
                        skip.insert(one->getUuid());
                    }
                }
                for (const auto conn : connIds(scene, id)) {
                    if (!scene->getState().isComponentValid(conn)) {
                        continue;
                    }
                    rmConns.push_back(conn);
                    for (const auto &one : collect(scene, {conn})) {
                        if (one) {
                            skip.insert(one->getUuid());
                        }
                    }
                }
            }
            return skip;
        }

        Status rewire(const std::shared_ptr<Canvas::ModuleSceneComponent> &mod,
                      const std::shared_ptr<Canvas::Scene> &scene) {
            const auto def =
                std::dynamic_pointer_cast<SimEngine::ModuleDefinition>(
                    mod ? mod->getCompDef() : nullptr);
            if (!mod || !scene || !def) {
                return Status::fail(
                    Err::invalid, "module data is not available for rewiring");
            }
            const auto &state = scene->getState();
            const auto input =
                state.getComponentByUuidSP<Canvas::SimulationSceneComponent>(
                    mod->getAssociatedInp());
            const auto output =
                state.getComponentByUuidSP<Canvas::SimulationSceneComponent>(
                    mod->getAssociatedOut());
            if (!input || !output) {
                return Status::fail(Err::invalid,
                                    "module input or output was not restored");
            }
            def->setInputId(input->getSimEngineId());
            def->setOutputId(output->getSimEngineId());
            return Status::ok();
        }

        Status reg(ProjectSession &session,
                   const std::shared_ptr<Canvas::Scene> &scene,
                   UUID parent) {
            if (!scene) {
                return Status::fail(Err::badArg, "module scene is null");
            }
            if (!session.scenes().getSceneWithId(scene->getSceneId())) {
                session.scenes().addScene(scene);
            }
            if (scene->getState().getParentSceneId() == UUID::null) {
                scene->getState().setParentSceneId(parent);
            }
            return Status::ok();
        }

        Status unreg(ProjectSession &session, UUID scene) {
            session.scenes().removeScene(scene);
            return Status::ok();
        }
    } // namespace

    ValResult<std::shared_ptr<Canvas::ModuleSceneComponent>>
    makeModule(ProjectSession &session,
               const std::shared_ptr<Canvas::Scene> &source,
               UUID net,
               std::string name) {
        if (!source || net == UUID::null) {
            return {.status = Status::fail(Err::badArg,
                                           "source scene or net is invalid")};
        }

        auto &page = Pages::MainPage::getInstance()->getState();
        auto &map = page.getNetIdToCompMap(source->getSceneId());
        if (!map.contains(net) || map.at(net).empty()) {
            return {.status = Status::fail(Err::invalid,
                                           "net has no scene components")};
        }

        UUID in = UUID::null;
        UUID out = UUID::null;
        auto made = Canvas::ModuleSceneComponent::createNew(in, out);
        if (made.empty()) {
            return {.status = Status::fail(Err::apply,
                                           "module factory returned no data")};
        }
        auto mod = made.front()->cast<Canvas::ModuleSceneComponent>();
        made.erase(made.begin());
        mod->setName(std::move(name));

        const auto modScene =
            session.scenes().getSceneWithId(mod->getSceneId());
        if (!modScene) {
            return {.status = Status::fail(Err::apply,
                                           "module scene was not created")};
        }

        std::vector<UUID> boundaryConns;
        const auto skip = boundarySkip(source, session, net, boundaryConns);
        const auto found = collect(source, map.at(net));
        std::vector<std::shared_ptr<Canvas::SceneComponent>> moved;
        for (const auto &comp : found) {
            if (comp && !skip.contains(comp->getUuid())) {
                moved.push_back(comp);
            }
        }

        auto tx = session.tx("Create module");
        auto status = tx.step(
            "Register module scene",
            [modScene, parent = source->getSceneId()](ProjectSession &s) {
                return reg(s, modScene, parent);
            },
            [id = modScene->getSceneId()](ProjectSession &s) {
                return unreg(s, id);
            },
            [modScene, parent = source->getSceneId()](ProjectSession &s) {
                return reg(s, modScene, parent);
            });
        if (!status) {
            return {.status = status};
        }

        for (const auto id : roots(modScene)) {
            const auto root =
                modScene->getState()
                    .getComponentByUuidSP<Canvas::SceneComponent>(id);
            status =
                tx.trackAdd(root, kids(modScene, root), modScene->getSceneId());
            if (!status) {
                return {.status = status};
            }
        }

        status = tx.step(
            "Rewire module",
            [mod, modScene](ProjectSession &) { return rewire(mod, modScene); },
            [](ProjectSession &) { return Status::ok(); },
            [mod, modScene](ProjectSession &) {
                return rewire(mod, modScene);
            });
        if (!status) {
            return {.status = status};
        }

        status = tx.addComp(mod, std::move(made), source->getSceneId());
        if (!status) {
            return {.status = status};
        }
        if (!boundaryConns.empty()) {
            status = tx.rmComp(std::move(boundaryConns), source->getSceneId());
            if (!status) {
                return {.status = status};
            }
        }

        status = tx.step(
            "Move module components",
            [source, modScene, moved](ProjectSession &) {
                moveComps(*source, *modScene, moved);
                return Status::ok();
            },
            [source, modScene, moved](ProjectSession &) {
                moveComps(*modScene, *source, moved);
                return Status::ok();
            });
        if (!status) {
            return {.status = status};
        }

        const auto result = tx.commit();
        return {.status = result.status, .val = result ? mod : nullptr};
    }

    Status rmModule(ProjectTx &tx,
                    const std::shared_ptr<Canvas::Scene> &parent,
                    UUID id) {
        if (!parent || id == UUID::null) {
            return Status::fail(Err::badArg, "module scene or id is invalid");
        }
        const auto mod =
            parent->getState()
                .getComponentByUuidSP<Canvas::ModuleSceneComponent>(id);
        if (!mod) {
            return Status::fail(Err::invalid, "module component was not found");
        }

        const auto session =
            GAppContext::getInstance().getSubSystem<ProjectSession>();
        if (!session) {
            return Status::fail(Err::invalid, "project session was not found");
        }
        const auto scene = session->scenes().getSceneWithId(mod->getSceneId());
        if (!scene) {
            return Status::fail(Err::invalid, "module scene was not found");
        }

        auto status = tx.rmComp(id, parent->getSceneId());
        if (!status) {
            return status;
        }

        status = tx.step(
            "Rewire module",
            [](ProjectSession &) { return Status::ok(); },
            [mod, scene](ProjectSession &) { return rewire(mod, scene); },
            [](ProjectSession &) { return Status::ok(); });
        if (!status) {
            return status;
        }

        status = tx.rmComp(roots(scene), scene->getSceneId());
        if (!status) {
            return status;
        }

        return tx.step(
            "Unregister module scene",
            [id = scene->getSceneId()](ProjectSession &s) {
                return unreg(s, id);
            },
            [scene, parentId = parent->getSceneId()](ProjectSession &s) {
                return reg(s, scene, parentId);
            },
            [id = scene->getSceneId()](ProjectSession &s) {
                return unreg(s, id);
            });
    }
} // namespace Bess::Edit
