#include "scene/commands/reparent_node_command.h"
#include "scene/scene.h"
#include "ui/ui_main/project_explorer.h" // Include ProjectExplorer to access state

namespace Bess::Canvas::Commands {
    ReparentNodeCommand::ReparentNodeCommand(const UUID &entityId, const UUID &newParentId)
        : m_entityId(entityId), m_newParentId(newParentId) {}

    bool ReparentNodeCommand::execute() {
        auto scene = Scene::instance();
        
        // Capture old parent on first run
        if (m_firstRun) {
            auto node = UI::ProjectExplorer::state.nodesLookup[m_entityId];
            if (node && node->parentId != UUID::null) {
                m_oldParentId = node->parentId;
            } else {
                m_oldParentId = UUID::null;
            }
            m_firstRun = false;
        }

        // Notify UI about the change
        scene->getEventDispatcher().trigger(Events::EntityReparentedEvent{m_entityId, m_newParentId});

        return true;
    }

    std::any ReparentNodeCommand::undo() {
        auto scene = Scene::instance();
        
        // Notify UI about the change (Revert)
        scene->getEventDispatcher().trigger(Events::EntityReparentedEvent{m_entityId, m_oldParentId});
        
        return {};
    }

    std::any ReparentNodeCommand::getResult() {
        return true;
    }
} // namespace Bess::Canvas::Commands


