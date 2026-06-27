#include "bess_core/commands/delete_component_command.h"

#include "bess_core/scene/scene.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include "common/logger.h"
#include <unordered_set>

namespace Bess::Cmd {
    DeleteCompCmd::DeleteCompCmd() {
        m_name = "DeleteComponentCmd";
    }

    DeleteCompCmd::DeleteCompCmd(const std::vector<UUID> &componentUuids,
                                 DeleteCompCmdCB callback)
        : m_componentUuids(componentUuids.begin(), componentUuids.end()),
          m_callback(std::move(callback)) {
        m_name = "DeleteComponentCmd";
    }

    bool DeleteCompCmd::execute(const CommandContext &context) {
        const auto commandContext = makeContext(context);
        if (!commandContext.scene) {
            return false;
        }

        m_deletedComponents.clear();
        const auto deletionOrder = buildDeletionOrder(commandContext);
        if (deletionOrder.empty()) {
            return false;
        }

        auto &sceneState = commandContext.scene->getState();
        m_deletedComponents.reserve(deletionOrder.size());
        for (const auto &componentId : deletionOrder) {
            const auto component = sceneState.getComponentByUuidSP(componentId);
            if (!component) {
                continue;
            }

            m_deletedComponents.push_back(component);
        }

        return removeStoredComponents(commandContext);
    }

    void DeleteCompCmd::undo(const CommandContext &context) {
        const auto commandContext = makeContext(context);
        if (!commandContext.scene || m_deletedComponents.empty()) {
            return;
        }

        restoreStoredComponents(commandContext);
        if (m_callback) {
            m_callback(true, m_deletedComponents);
        }
    }

    void DeleteCompCmd::redo(const CommandContext &context) {
        const auto commandContext = makeContext(context);
        if (!commandContext.scene || m_deletedComponents.empty()) {
            return;
        }

        removeStoredComponents(commandContext);
        if (m_callback) {
            m_callback(false, m_deletedComponents);
        }
    }

    const std::vector<std::shared_ptr<Canvas::SceneComponent>> &
    DeleteCompCmd::getDeletedComponents() const {
        return m_deletedComponents;
    }

    std::vector<UUID>
    DeleteCompCmd::buildDeletionOrder(const CommandContext &context) const {
        std::vector<UUID> deletionOrder;
        if (!context.scene) {
            return deletionOrder;
        }

        std::unordered_set<UUID> visited;
        const auto collect = [&](const UUID &componentId,
                                 const auto &self) -> void {
            if (visited.contains(componentId)) {
                return;
            }

            const auto component =
                context.scene->getState().getComponentByUuidSP(componentId);
            if (!component) {
                return;
            }

            visited.insert(componentId);
            for (const auto &dependantId : getSceneComponentDependantsWithHooks(
                     context.scene, component, context.componentHooks)) {
                self(dependantId, self);
            }
            deletionOrder.push_back(componentId);
        };

        for (const auto &componentId : m_componentUuids) {
            collect(componentId, collect);
        }

        sortSceneComponentDeletionOrderWithHooks(
            context.scene, deletionOrder, context.componentHooks);
        return deletionOrder;
    }

    bool DeleteCompCmd::removeStoredComponents(const CommandContext &context) {
        if (!context.scene) {
            return false;
        }

        auto &sceneState = context.scene->getState();
        bool removedAny = false;
        for (const auto &component : m_deletedComponents) {
            if (!component ||
                !sceneState.isComponentValid(component->getUuid())) {
                continue;
            }

            removeSceneComponentWithHooks(
                context.scene, component, UUID::master, context.componentHooks);
            removedAny = true;
        }

        return removedAny;
    }

    void DeleteCompCmd::restoreStoredComponents(const CommandContext &context) {
        auto &sceneState = context.scene->getState();

        for (auto it = m_deletedComponents.rbegin();
             it != m_deletedComponents.rend();
             ++it) {
            const auto &component = *it;
            if (!component) {
                continue;
            }

            addSceneComponentWithHooks(
                context.scene,
                component,
                {.setZ = false, .triggerAttach = false, .dispatchEvent = true},
                context.componentHooks);

            const auto parentId = component->getParentComponent();
            if (parentId == UUID::null ||
                !sceneState.isComponentValid(component->getUuid()) ||
                !sceneState.isComponentValid(parentId)) {
                continue;
            }

            const auto parent = sceneState.getComponentByUuid(parentId);
            if (parent &&
                !parent->getChildComponents().contains(component->getUuid())) {
                sceneState.attachChild(parentId, component->getUuid());
            }
        }

        for (auto it = m_deletedComponents.rbegin();
             it != m_deletedComponents.rend();
             ++it) {
            if (*it && sceneState.isComponentValid((*it)->getUuid())) {
                (*it)->onAttach(sceneState);
            }
        }
    }
} // namespace Bess::Cmd
