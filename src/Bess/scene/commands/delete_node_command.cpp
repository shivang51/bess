#include "scene/commands/delete_node_command.h"
#include "commands/composite_command.h"
#include "scene/commands/delete_comp_command.h"
#include "scene/scene.h"
#include "ui/ui_main/project_explorer.h"

namespace Bess::Canvas::Commands {
    DeleteNodeCommand::DeleteNodeCommand(const std::vector<UUID> &nodeIds)
        : m_nodeIds(nodeIds) {}

    bool DeleteNodeCommand::execute() {
        auto scene = Scene::instance();
        auto &state = UI::ProjectExplorer::state;

        std::vector<UUID> sceneIds, groupIds;

        // Recursive helper to collect entities
        std::function<void(UUID)> collectEntities = [&](UUID nodeId) {
            if (state.nodesLookup.contains(nodeId)) {
                auto node = state.nodesLookup[nodeId];
                if (node->isGroup) {
                    groupIds.push_back(node->nodeId);
                    for (const auto &child : node->children) {
                        collectEntities(child->nodeId);
                    }
                } else if (node->sceneId != UUID::null) {
                    sceneIds.push_back(node->sceneId);
                } else {
                    BESS_ASSERT(false, "Node is neither group nor has a valid scene ID");
                }
            }
        };

        for (auto id : m_nodeIds) {
            collectEntities(id);
        }

        for (const auto &nodeId : groupIds)
            state.removeNode(state.nodesLookup[nodeId]);

        if (!sceneIds.empty()) {
            auto res = scene->getCmdManager().execute<DeleteCompCommand, std::string>(sceneIds);
            return res.has_value();
        }

        return true;
    }

    std::any DeleteNodeCommand::undo() {
        return "";
    }

    std::any DeleteNodeCommand::getResult() {
        return std::string("");
    }
} // namespace Bess::Canvas::Commands
