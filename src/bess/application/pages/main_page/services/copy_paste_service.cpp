#include "copy_paste_service.h"
#include "common/bess_uuid.h"
#include "component_catalog.h"
#include "macro_command.h"
#include "pages/main_page/cmds/add_comp_cmd.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/scene_components/module_scene_component.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "simulation_engine.h"
#include <unordered_map>

namespace Bess::Svc::CopyPaste {
    void Context::copy(const std::shared_ptr<Canvas::Scene> &scene) {
        BESS_ASSERT(scene, "[CopyPaste] Pass valid scene to copy function");

        clear();

        auto &simEngine = SimEngine::SimulationEngine::instance();
        const auto &catalogInstance = SimEngine::ComponentCatalog::instance();

        const auto &sceneState = scene->getState();
        const auto &selComponents = sceneState.getSelectedComponents() | std::views::keys;

        m_copiedScene = scene;

        for (const auto &selId : selComponents) {
            const auto comp = sceneState.getComponentByUuid(selId);
            const auto type = comp->getType();

            CopiedEntity entity{};
            entity.type = type;

            switch (type) {
            case Canvas::SceneComponentType::simulation:
            case Canvas::SceneComponentType::module: {
                const auto casted = comp->cast<Canvas::SimulationSceneComponent>();
                entity.data = Svc::CopyPaste::ETSimComp{casted};
            } break;
            case Canvas::SceneComponentType::nonSimulation: {
                const auto casted = comp->cast<Canvas::NonSimSceneComponent>();
                entity.data = Svc::CopyPaste::ETNonSimComp{casted->getTypeIndex(), casted};
            } break;
            case Canvas::SceneComponentType::connection: {
                const auto casted = comp->cast<Canvas::ConnectionSceneComponent>();
                entity.data = Svc::CopyPaste::ETConnection{casted};
            } break;
            default:
                continue;
                break;
            }

            entity.pos = comp->getTransform().position;

            addEntity(entity);
        }
    }

    std::unordered_map<UUID, UUID> Context::paste(const std::shared_ptr<Canvas::Scene> &targetScene) {
        if (m_entities.empty())
            return {};

        BESS_ASSERT(m_copiedScene,
                    "Copied Scene is invalid, although entities were copied");

        calcCenter();

        auto &cmdSystem = Pages::MainPage::getInstance()->getState().getCommandSystem();
        const auto currentCmdSystemScene = cmdSystem.getScene();
        cmdSystem.setScene(targetScene.get());
        auto macroCmd = std::make_unique<Cmd::MacroCommand>();

        const auto &newCenter = targetScene->getSceneMousePos();
        std::vector<Svc::CopyPaste::CopiedEntity> connEntites;

        std::unordered_map<UUID, UUID> ogToClonedIdMap;

        for (auto &entity : m_entities) {
            const auto pos = newCenter + entity.pos - m_center;
            if (entity.type == Canvas::SceneComponentType::simulation ||
                entity.type == Canvas::SceneComponentType::module) {
                const auto &entityData = std::get<Svc::CopyPaste::ETSimComp>(entity.data);
                auto clonedComponents = entityData.comp->clone(m_copiedScene->getState());
                BESS_ASSERT(!clonedComponents.empty(),
                            "Simulation clone returned no components");

                if (entity.type == Canvas::SceneComponentType::module) {
                    BESS_ASSERT(std::dynamic_pointer_cast<Canvas::ModuleSceneComponent>(
                                    clonedComponents.front()),
                                "Module clone did not return a module component first");
                } else {
                    BESS_ASSERT(std::dynamic_pointer_cast<Canvas::SimulationSceneComponent>(
                                    clonedComponents.front()),
                                "Simulation clone did not return a simulation component first");
                }

                auto clonedComp = clonedComponents.front()->cast<Canvas::SimulationSceneComponent>();
                clonedComponents.erase(clonedComponents.begin());
                BESS_ASSERT(clonedComponents.size() == entityData.comp->getChildComponents().size(),
                            "[Paste] Not all child comps got cloned");

                clonedComp->getTransform().position.x = pos.x;
                clonedComp->getTransform().position.y = pos.y;

                ogToClonedIdMap[entityData.comp->getUuid()] = clonedComp->getUuid();

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

                auto addCmd = std::make_unique<Cmd::AddCompCmd<Canvas::SimulationSceneComponent>>(
                    clonedComp,
                    clonedComponents);
                macroCmd->addCommand(std::move(addCmd));
            } else if (entity.type == Canvas::SceneComponentType::nonSimulation) {
                const auto &entityData = std::get<Svc::CopyPaste::ETNonSimComp>(entity.data);
                auto clonedComponents = entityData.comp->clone(m_copiedScene->getState());
                BESS_ASSERT(!clonedComponents.empty(), "Non-simulation clone returned no components");

                auto inst = std::dynamic_pointer_cast<Canvas::NonSimSceneComponent>(clonedComponents.front());
                BESS_ASSERT(inst, "Non-simulation clone did not return a non-simulation component first");
                clonedComponents.erase(clonedComponents.begin());
                BESS_ASSERT(clonedComponents.size() == entityData.comp->getChildComponents().size(),
                            "[Paste] Not all child comps got cloned");
                inst->getTransform().position.x = pos.x;
                inst->getTransform().position.y = pos.y;

                ogToClonedIdMap[entityData.comp->getUuid()] = inst->getUuid();

                size_t idx = 0;
                for (const auto &ogId : entityData.comp->getChildComponents()) {
                    const auto &clonedId = clonedComponents[idx]->getUuid();
                    ogToClonedIdMap[ogId] = clonedId;
                    idx++;
                }

                auto addCmd = std::make_unique<Cmd::AddCompCmd<Canvas::NonSimSceneComponent>>(inst, clonedComponents);
                macroCmd->addCommand(std::move(addCmd));
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
                const auto &entityData = std::get<Svc::CopyPaste::ETConnection>(connEntity.data);
                const auto &ogConn = entityData.conn->cast<Canvas::ConnectionSceneComponent>();

                if (!ogToClonedIdMap.contains(ogConn->getStartSlot()) ||
                    !ogToClonedIdMap.contains(ogConn->getEndSlot())) {
                    delayedConnections.push_back(connEntity);
                    continue;
                }

                auto clonedComps = ogConn->cloneConn(m_copiedScene->getState(),
                                                     ogToClonedIdMap);

                for (const auto &comp : clonedComps) {
                    // either it can be a joint or a conn
                    auto addCmd = std::make_unique<Cmd::AddCompCmd<Canvas::SceneComponent>>(comp);
                    macroCmd->addCommand(std::move(addCmd));
                }
            }

            connEntites.clear();
            for (const auto &entity : delayedConnections) {
                const auto &entityData = std::get<Svc::CopyPaste::ETConnection>(entity.data);
                if (!ogToClonedIdMap.contains(entityData.conn->getUuid())) {
                    connEntites.push_back(entity);
                }
            }
        } while (!connEntites.empty() && prevSize < connEntites.size());

        cmdSystem.execute(std::move(macroCmd));

        cmdSystem.setScene(currentCmdSystemScene);

        return ogToClonedIdMap;
    }

    Context &Context::instance() {
        static Context context;
        return context;
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

    void Context::init() {
        clear();
    }

    void Context::destroy() {
        clear();
    }

} // namespace Bess::Svc::CopyPaste
