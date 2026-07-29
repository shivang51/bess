#include "bess_core/copy_paste_service.h"
#include "bess_core/g_app_context.h"
#include "bess_core/scene/scene_state/components/scene_component.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include "bess_core/scene_driver.h"
#include "common/bess_uuid.h"
#include "pages/main_page/scene_components/module_scene_component.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "project_session/project_session.h"
#include "simulation_engine.h"
#include <unordered_map>

namespace Bess::Svc::CopyPaste {
    namespace {
        void bindModuleSceneToParent(
            const std::shared_ptr<Canvas::ModuleSceneComponent> &module,
            const std::shared_ptr<Canvas::Scene> &parentScene) {
            BESS_ASSERT(module, "[CopyPaste] Module clone must be valid");
            BESS_ASSERT(parentScene,
                        "[CopyPaste] Module parent scene must be valid");

            if (!module || !parentScene) {
                return;
            }

            const auto session = GAppContext::getInstance()
                                     .getSubSystem<ProjectSession>();
            const auto moduleScene =
                session->scenes().getSceneWithId(module->getSceneId());

            BESS_ASSERT(moduleScene,
                        "[CopyPaste] Cloned module scene was not registered");
            if (!moduleScene) {
                return;
            }

            auto &moduleSceneState = moduleScene->getState();
            moduleSceneState.setIsRootScene(false);
            moduleSceneState.setParentSceneId(parentScene->getSceneId());
            moduleSceneState.setModuleId(module->getUuid());
        }
    } // namespace

    void Context::copy(const std::shared_ptr<Canvas::Scene> &scene) {
        BESS_ASSERT(scene, "[CopyPaste] Pass valid scene to copy function");
        if (!scene) {
            BESS_ERROR("[CopyPaste] Cannot copy from a null scene");
            return;
        }

        clear();

        const auto &sceneState = scene->getState();

        m_copiedScene = scene;

        for (const auto &[selId, selected] :
             sceneState.getSelectedComponents()) {
            if (!selected) {
                continue;
            }
            addEntityFromComponent(sceneState, selId);
        }

        BESS_DEBUG("Copied {} entities from scene {}",
                   m_entities.size(),
                   (uint64_t)sceneState.getSceneId());
    }

    void Context::copyScene(const std::shared_ptr<Canvas::Scene> &scene) {
        BESS_ASSERT(scene,
                    "[CopyPaste] Pass valid scene to copyScene function");
        if (!scene) {
            BESS_ERROR("[CopyPaste] Cannot copy a null scene");
            return;
        }

        clear();

        const auto &sceneState = scene->getState();
        m_copiedScene = scene;

        for (const auto &componentId : sceneState.getRootComponents()) {
            addEntityFromComponent(sceneState, componentId);
        }

        BESS_DEBUG("Copied full scene with {} root entities from scene {}",
                   m_entities.size(),
                   (uint64_t)sceneState.getSceneId());
    }

    std::unordered_map<UUID, UUID>
    Context::paste(const std::shared_ptr<Canvas::Scene> &targetScene,
                   const glm::vec2 &targetPos,
                   bool recordHistory) {
        if (m_entities.empty())
            return {};

        BESS_ASSERT(targetScene,
                    "[CopyPaste] Pass valid scene to paste function");
        if (!targetScene) {
            BESS_ERROR("[CopyPaste] Cannot paste into a null scene");
            return {};
        }

        BESS_ASSERT(m_copiedScene,
                    "Copied Scene is invalid, although entities were copied");
        if (!m_copiedScene) {
            BESS_ERROR(
                "[CopyPaste] Cannot paste without a copied source scene");
            return {};
        }

        calcCenter();

        const auto session =
            GAppContext::getInstance().getSubSystem<ProjectSession>();
        if (!session) {
            BESS_ERROR("[CopyPaste] Project session is unavailable");
            return {};
        }

        auto tx = session->tx(
            "Paste", {.empty = true, .hist = recordHistory});
        const auto add = [&](const std::shared_ptr<Canvas::SceneComponent> &comp,
                             std::vector<
                                 std::shared_ptr<Canvas::SceneComponent>> kids =
                                 {}) {
            const auto status = tx.addComp(
                comp, std::move(kids), targetScene->getSceneId());
            if (!status) {
                BESS_ERROR("[CopyPaste] Could not stage paste: {}",
                           status.msg());
            }
            return status.isOk();
        };

        std::vector<Svc::CopyPaste::CopiedEntity> connEntites;

        std::unordered_map<UUID, UUID> ogToClonedIdMap;

        for (auto &entity : m_entities) {
            const auto pos = targetPos + entity.pos - m_center;
            if (entity.type == Canvas::SceneComponentType::simulation ||
                entity.type == Canvas::SceneComponentType::module) {
                const auto &entityData =
                    std::get<Svc::CopyPaste::ETSimComp>(entity.data);
                auto clonedComponents =
                    entityData.comp->clone(m_copiedScene->getState());
                BESS_ASSERT(!clonedComponents.empty(),
                            "Simulation clone returned no components");
                if (clonedComponents.empty()) {
                    BESS_ERROR("[CopyPaste] Skipping component {} because "
                               "clone returned no components",
                               (uint64_t)entityData.comp->getUuid());
                    continue;
                }

                const auto clonedPrimary = clonedComponents.front();
                const auto clonedModule =
                    std::dynamic_pointer_cast<Canvas::ModuleSceneComponent>(
                        clonedPrimary);

                if (entity.type == Canvas::SceneComponentType::module) {
                    BESS_ASSERT(clonedModule,
                                "Module clone did not return a module "
                                "component first");
                    if (!clonedModule) {
                        BESS_ERROR("[CopyPaste] Skipping module {} because "
                                   "clone returned the wrong component type",
                                   (uint64_t)entityData.comp->getUuid());
                        continue;
                    }
                } else {
                    BESS_ASSERT(
                        std::dynamic_pointer_cast<
                            Canvas::SimulationSceneComponent>(clonedPrimary),
                        "Simulation clone did not return a simulation "
                        "component first");
                }

                auto clonedComp =
                    std::dynamic_pointer_cast<Canvas::SimulationSceneComponent>(
                        clonedPrimary);
                if (!clonedComp) {
                    BESS_ERROR("[CopyPaste] Skipping component {} because "
                               "clone returned the wrong component type",
                               (uint64_t)entityData.comp->getUuid());
                    continue;
                }
                clonedComponents.erase(clonedComponents.begin());
                BESS_ASSERT(clonedComponents.size() ==
                                entityData.comp->getChildComponents().size(),
                            "[Paste] Not all child comps got cloned");
                if (clonedComponents.size() !=
                    entityData.comp->getChildComponents().size()) {
                    BESS_ERROR("[CopyPaste] Skipping component {} because "
                               "clone returned {} children, expected {}",
                               (uint64_t)entityData.comp->getUuid(),
                               clonedComponents.size(),
                               entityData.comp->getChildComponents().size());
                    continue;
                }

                BESS_ASSERT(clonedComp->getInputSlots().size() ==
                                entityData.comp->getInputSlots().size(),
                            "[Paste] Not all input slots got cloned");
                BESS_ASSERT(clonedComp->getOutputSlots().size() ==
                                entityData.comp->getOutputSlots().size(),
                            "[Paste] Not all output slots got cloned");
                if (clonedComp->getInputSlots().size() !=
                        entityData.comp->getInputSlots().size() ||
                    clonedComp->getOutputSlots().size() !=
                        entityData.comp->getOutputSlots().size()) {
                    BESS_ERROR("[CopyPaste] Skipping component {} because "
                               "clone returned mismatched slot counts",
                               (uint64_t)entityData.comp->getUuid());
                    continue;
                }

                clonedComp->getTransform().position.x = pos.x;
                clonedComp->getTransform().position.y = pos.y;

                if (clonedModule) {
                    bindModuleSceneToParent(clonedModule, targetScene);
                }

                ogToClonedIdMap[entityData.comp->getUuid()] =
                    clonedComp->getUuid();

                size_t idx = 0;
                for (const auto &ogId : entityData.comp->getInputSlots()) {
                    const auto &clonedId = clonedComp->getInputSlots()[idx];
                    ogToClonedIdMap[ogId] = clonedId;
                    idx++;
                }

                idx = 0;
                for (const auto &ogId : entityData.comp->getOutputSlots()) {
                    const auto &clonedId = clonedComp->getOutputSlots()[idx];
                    ogToClonedIdMap[ogId] = clonedId;
                    idx++;
                }

                if (!add(clonedComp, std::move(clonedComponents))) {
                    tx.cancel();
                    return {};
                }
            } else if (entity.type ==
                       Canvas::SceneComponentType::nonSimulation) {
                const auto &entityData =
                    std::get<Svc::CopyPaste::ETNonSimComp>(entity.data);
                auto clonedComponents =
                    entityData.comp->clone(m_copiedScene->getState());
                BESS_ASSERT(!clonedComponents.empty(),
                            "Non-simulation clone returned no components");
                if (clonedComponents.empty()) {
                    BESS_ERROR("[CopyPaste] Skipping component {} because "
                               "clone returned no components",
                               (uint64_t)entityData.comp->getUuid());
                    continue;
                }

                auto inst =
                    std::dynamic_pointer_cast<Canvas::NonSimSceneComponent>(
                        clonedComponents.front());
                BESS_ASSERT(inst,
                            "Non-simulation clone did not return a "
                            "non-simulation component first");
                if (!inst) {
                    BESS_ERROR("[CopyPaste] Skipping component {} because "
                               "clone returned the wrong component type",
                               (uint64_t)entityData.comp->getUuid());
                    continue;
                }
                clonedComponents.erase(clonedComponents.begin());
                BESS_ASSERT(clonedComponents.size() ==
                                entityData.comp->getChildComponents().size(),
                            "[Paste] Not all child comps got cloned");
                if (clonedComponents.size() !=
                    entityData.comp->getChildComponents().size()) {
                    BESS_ERROR("[CopyPaste] Skipping component {} because "
                               "clone returned {} children, expected {}",
                               (uint64_t)entityData.comp->getUuid(),
                               clonedComponents.size(),
                               entityData.comp->getChildComponents().size());
                    continue;
                }
                inst->getTransform().position.x = pos.x;
                inst->getTransform().position.y = pos.y;

                ogToClonedIdMap[entityData.comp->getUuid()] = inst->getUuid();

                size_t idx = 0;
                for (const auto &ogId : entityData.comp->getChildComponents()) {
                    const auto &clonedId = clonedComponents[idx]->getUuid();
                    ogToClonedIdMap[ogId] = clonedId;
                    idx++;
                }

                if (!add(inst, std::move(clonedComponents))) {
                    tx.cancel();
                    return {};
                }
            } else if (entity.type == Canvas::SceneComponentType::connection) {
                connEntites.push_back(entity);
            } else {
                BESS_ERROR("Trying to paste unknown type {}", (int)entity.type);
                BESS_ASSERT(false, "Trying to paste unknown type");
            }
        }

        size_t prevSize = 0;
        do {
            prevSize = connEntites.size();
            std::vector<Svc::CopyPaste::CopiedEntity> delayedConnections;

            for (const auto &connEntity : connEntites) {
                const auto &entityData =
                    std::get<Svc::CopyPaste::ETConnection>(connEntity.data);
                const auto &ogConn =
                    entityData.conn->cast<Canvas::ConnectionSceneComponent>();

                if (!ogToClonedIdMap.contains(ogConn->getStartSlot()) ||
                    !ogToClonedIdMap.contains(ogConn->getEndSlot())) {
                    delayedConnections.push_back(connEntity);
                    continue;
                }

                auto clonedComps = ogConn->cloneConn(m_copiedScene->getState(),
                                                     ogToClonedIdMap);

                for (const auto &comp : clonedComps) {
                    // either it can be a joint or a conn
                    if (!add(comp)) {
                        tx.cancel();
                        return {};
                    }
                }
            }

            connEntites.clear();
            for (const auto &entity : delayedConnections) {
                const auto &entityData =
                    std::get<Svc::CopyPaste::ETConnection>(entity.data);
                if (!ogToClonedIdMap.contains(entityData.conn->getUuid())) {
                    connEntites.push_back(entity);
                }
            }
        } while (!connEntites.empty() && connEntites.size() < prevSize);

        const auto result = tx.commit();
        if (!result) {
            BESS_ERROR("[CopyPaste] Paste failed: {}",
                       result.status.msg());
            return {};
        }

        BESS_DEBUG("Pasted {} entities into scene {}",
                   m_entities.size(),
                   (uint64_t)targetScene->getState().getSceneId());

        return ogToClonedIdMap;
    }

    bool Context::addEntityFromComponent(const Canvas::SceneState &sceneState,
                                         const UUID &componentId) {
        const auto comp = sceneState.getComponentByUuidSP(componentId);
        if (!comp) {
            BESS_WARN("[CopyPaste] Component {} was not found while copying",
                      (uint64_t)componentId);
            return false;
        }

        const auto type = comp->getType();

        CopiedEntity entity{};
        entity.type = type;

        switch (type) {
        case Canvas::SceneComponentType::simulation:
        case Canvas::SceneComponentType::module: {
            const auto casted =
                std::dynamic_pointer_cast<Canvas::SimulationSceneComponent>(
                    comp);
            BESS_ASSERT(casted,
                        "[CopyPaste] Expected simulation component while "
                        "copying");
            if (!casted) {
                return false;
            }
            entity.data = Svc::CopyPaste::ETSimComp{casted};
        } break;
        case Canvas::SceneComponentType::nonSimulation: {
            const auto casted =
                std::dynamic_pointer_cast<Canvas::NonSimSceneComponent>(comp);
            BESS_ASSERT(casted,
                        "[CopyPaste] Expected non-simulation component while "
                        "copying");
            if (!casted) {
                return false;
            }
            entity.data =
                Svc::CopyPaste::ETNonSimComp{casted->getTypeIndex(), casted};
        } break;
        case Canvas::SceneComponentType::connection: {
            const auto casted =
                std::dynamic_pointer_cast<Canvas::ConnectionSceneComponent>(
                    comp);
            BESS_ASSERT(casted,
                        "[CopyPaste] Expected connection component while "
                        "copying");
            if (!casted) {
                return false;
            }
            entity.data = Svc::CopyPaste::ETConnection{casted};
        } break;
        default:
            return false;
        }

        entity.pos = comp->getTransform().position;

        addEntity(entity);
        return true;
    }

    void Context::addEntity(const CopiedEntity &entity) {
        m_entities.push_back(entity);
    }

    void Context::clear() {
        m_entities.clear();
        m_copiedScene = nullptr;
    }

    void Context::calcCenter() {
        if (m_entities.empty())
            return;
        if (m_entities.size() == 1)
            m_center = m_entities.front().pos;

        glm::vec2 sumPos{0.f, 0.f};

        for (const auto &ent : m_entities) {
            sumPos += ent.pos;
        }

        m_center = sumPos / (float)m_entities.size();
    }

    void Context::onInit() {
        clear();
    }

    void Context::onDestroy() {
        clear();
    }

} // namespace Bess::Svc::CopyPaste
