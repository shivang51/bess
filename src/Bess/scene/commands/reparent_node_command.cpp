#include "scene/commands/reparent_node_command.h"
#include "scene/scene.h"
#include "ui/ui_main/project_explorer.h" // Include ProjectExplorer to access state
#include <cstdint>

namespace Bess::Canvas::Commands {
    ReparentNodeCommand::ReparentNodeCommand(const UUID &entityId, const UUID &newParentId)
        : m_entityId(entityId), m_newParentId(newParentId) {}

    bool ReparentNodeCommand::execute() {
        return false;
        // auto scene = Scene::instance();
        //
        // if (m_firstRun) {
        //     auto &state = UI::ProjectExplorer::state;
        //     auto node = state.nodesLookup.at(m_entityId);
        //     m_oldParentId = node->parentId;
        //     state.moveNode(m_entityId, m_newParentId);
        //     m_firstRun = false;
        // }
        //
        // // scene->getEventDispatcher().trigger(Events::EntityReparentedEvent{m_entityId, m_newParentId});
        //
        // return true;
    }

    std::any ReparentNodeCommand::undo() {
        return false;
        // auto scene = Scene::instance();
        //
        // UI::ProjectExplorer::state.moveNode(m_entityId, m_oldParentId);
        // // scene->getEventDispatcher().trigger(Events::EntityReparentedEvent{m_entityId, m_oldParentId});
        //
        // return {};
    }

    std::any ReparentNodeCommand::getResult() {
        return true;
    }
} // namespace Bess::Canvas::Commands
