#pragma once

#include "command.h"
#include "common/bess_uuid.h"
#include "common/logger.h"
#include "pages/main_page/scene_components/connection_scene_component.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "pages/main_page/services/connection_service.h"
#include "scene/scene.h"
#include "scene/scene_state/components/scene_component.h"
#include <algorithm>
#include <vector>

namespace Bess::Cmd {
    // bool indicated if its undo/redo, true if its undo
    // vector is list of processed comps
    typedef std::function<void(bool,
                               const std::vector<std::shared_ptr<Canvas::SceneComponent>> &)>
        DeleteCompCmdCB;

    class DeleteCompCmd : public Bess::Cmd::Command {
      public:
        DeleteCompCmd() {
            m_name = "DeleteComponentCmd";
        }

        DeleteCompCmd(const std::vector<UUID> &compUuids,
                      const DeleteCompCmdCB &callback = nullptr) : m_callback(callback) {
            m_compUuids = std::set<UUID>(compUuids.begin(), compUuids.end());
            m_name = "DeleteComponentCmd";
        }

        bool execute(Canvas::Scene *scene, SimEngine::SimulationEngine *simEngine) override {
            m_deletedComponents.clear();
            auto &sceneState = scene->getState();

            std::unordered_set<UUID> visited;
            std::vector<UUID> deletionOrder;

            // topological sort
            // identifies everything that must go, dependants first.
            std::function<void(const UUID &)> collect = [&](const UUID &uuid) {
                if (visited.contains(uuid))
                    return;
                auto comp = sceneState.getComponentByUuid(uuid);
                if (!comp)
                    return;

                visited.insert(uuid);
                for (const auto &depUuid : comp->getDependants(sceneState)) {
                    collect(depUuid);
                }
                deletionOrder.push_back(uuid);
            };

            for (const auto &startUuid : m_compUuids) {
                collect(startUuid);
            }

            std::vector<std::shared_ptr<Canvas::ConnectionSceneComponent>> connections;

            // extract all connections from the deletion list
            for (auto it = deletionOrder.begin(); it != deletionOrder.end();) {
                auto comp = sceneState.getComponentByUuid(*it);
                if (comp && comp->getType() == Canvas::SceneComponentType::connection) {
                    connections.push_back(comp->cast<Canvas::ConnectionSceneComponent>());
                    it = deletionOrder.erase(it); // Remove from the main list
                } else {
                    ++it;
                }
            }

            // Sort connections:
            // sort by slot Index DESCENDING (3 -> 2 -> 1)
            std::ranges::sort(connections, [&sceneState](const auto &a, const auto &b) {
                if (a->getParentComponent() != b->getParentComponent()) {
                    return true;
                }

                const auto getMaxSlotIdx = [&](const auto &conn) {
                    const auto &slotA = conn->getStartSlot();
                    const auto &slotB = conn->getEndSlot();
                    const auto &slotAComp = sceneState.getComponentByUuid<
                        Canvas::SlotSceneComponent>(slotA);
                    const auto &slotBComp = sceneState.getComponentByUuid<
                        Canvas::SlotSceneComponent>(slotB);

                    const auto &idxA = slotAComp ? slotAComp->getIndex() : 0;
                    const auto &idxB = slotBComp ? slotBComp->getIndex() : 0;
                    return std::max(idxA, idxB);
                };

                const auto &maxSlotIdxA = getMaxSlotIdx(a);
                const auto &maxSlotIdxB = getMaxSlotIdx(b);

                return maxSlotIdxA > maxSlotIdxB; // Descending order
            });

            // Now execute the deletion in the safe order
            for (auto conn : connections) {

                Svc::SvcConnection::instance().removeConnection(conn);

                m_deletedComponents.push_back(std::move(conn));
            }

            //  delete the rest (slots, nodes, etc.)
            for (const auto &uuid : deletionOrder) {
                auto comp = sceneState.getComponentByUuid(uuid);
                if (!comp)
                    continue;

                BESS_DEBUG("Removing Component {} ({})", (uint64_t)uuid, comp->getName());

                sceneState.removeComponent(uuid, UUID::master);
                m_deletedComponents.push_back(std::move(comp));
            }

            return !m_deletedComponents.empty();
        }

        void undo(Canvas::Scene *scene,
                  SimEngine::SimulationEngine *simEngine) override {
            auto &sceneState = scene->getState();

            auto &connectionsSvc = Svc::SvcConnection::instance();
            std::vector<std::shared_ptr<Canvas::SceneComponent>> groupComps;
            for (const auto &deletedComponent : std::ranges::reverse_view(m_deletedComponents)) {
                BESS_DEBUG("Restoring component: {} with uuid {}", deletedComponent->getName(),
                           (uint64_t)deletedComponent->getUuid());
                if (deletedComponent->getType() == Canvas::SceneComponentType::connection) {
                    connectionsSvc.addConnection(deletedComponent->cast<Canvas::ConnectionSceneComponent>());
                } else {
                    sceneState.addComponent(deletedComponent,
                                            deletedComponent->getType() != Canvas::SceneComponentType::group);
                }

                if (deletedComponent->getType() == Canvas::SceneComponentType::group) {
                    groupComps.push_back(deletedComponent);
                }

                const auto &parentUuid = deletedComponent->getParentComponent();

                if (parentUuid == UUID::null)
                    continue;

                const auto &parentComp = sceneState.getComponentByUuid(parentUuid);

                if (!parentComp) {
                    BESS_ERROR("Parent  {} not found for {} during undo of del cmd.",
                               (uint64_t)parentUuid, deletedComponent->getName());
                    BESS_ASSERT(false, "Parent component not found during undo of delete command");
                    continue;
                }

                sceneState.attachChild(deletedComponent->getParentComponent(),
                                       deletedComponent->getUuid());
            }

            for (const auto &groupComp : groupComps) {
                groupComp->onAttach(sceneState);
            }

            if (m_callback) {
                m_callback(true, m_deletedComponents);
            }
        }

        void redo(Canvas::Scene *scene,
                  SimEngine::SimulationEngine *simEngine) override {
            auto &connectionsSvc = Svc::SvcConnection::instance();

            for (const auto &comp : m_deletedComponents) {
                if (comp->getType() == Canvas::SceneComponentType::connection) {
                    connectionsSvc.removeConnection(comp->cast<Canvas::ConnectionSceneComponent>());
                } else {
                    scene->getState().removeComponent(comp->getUuid(), UUID::master);
                }
            }

            if (m_callback) {
                m_callback(false, m_deletedComponents);
            }
        }

      private:
        std::set<UUID> m_compUuids;
        std::vector<std::shared_ptr<Canvas::SceneComponent>> m_deletedComponents;
        DeleteCompCmdCB m_callback = nullptr;
    };
} // namespace Bess::Cmd
