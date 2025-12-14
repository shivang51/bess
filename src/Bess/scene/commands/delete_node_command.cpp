#include "scene/commands/delete_node_command.h"
#include "scene/commands/delete_comp_command.h"
#include "commands/composite_command.h"
#include "scene/scene.h"
#include "ui/ui_main/project_explorer.h"

namespace Bess::Canvas::Commands {
    DeleteNodeCommand::DeleteNodeCommand(const std::vector<UUID> &nodeIds) 
        : m_nodeIds(nodeIds) {}

    bool DeleteNodeCommand::execute() {
        // Build composite command
        auto composite = std::make_unique<SimEngine::Commands::CompositeCommand>();
        
        std::vector<UUID> entityIds;
        auto scene = Scene::instance();
        auto &state = UI::ProjectExplorer::state;

        // Recursive helper to collect entities
        std::function<void(UUID)> collectEntities = [&](UUID nodeId) {
            if (state.nodesLookup.contains(nodeId)) {
                auto node = state.nodesLookup[nodeId];
                
                // If it's an entity leaf
                if (!node->isGroup && scene->isEntityValid(node->sceneId)) {
                    entityIds.push_back(node->sceneId);
                }
                
                // Recurse for children
                for (auto &child : node->children) {
                    collectEntities(child->nodeId);
                }
            } else if (scene->isEntityValid(nodeId)) {
                // Fallback if ID is direct entity ID
                entityIds.push_back(nodeId);
            }
        };

        for (auto id : m_nodeIds) {
            collectEntities(id);
        }

        // 1. Create Command to delete entities
        if (!entityIds.empty()) {
            composite->addCommand(std::make_unique<DeleteCompCommand>(entityIds));
        }

        // 2. Handle UI Groups deletion
        // We only need to remove the top-level groups passed in m_nodeIds.
        // Their children (nodes) are removed implicitly from UI structure when parent is removed.
        // (Entities were deleted above).
        std::vector<UUID> groupsToRemove;
        for (auto id : m_nodeIds) {
            if (state.nodesLookup.contains(id)) {
                auto node = state.nodesLookup[id];
                if (node->isGroup) {
                    groupsToRemove.push_back(id);
                }
            }
        }

        class RemoveUIGroupsCommand : public Command {
            std::vector<UUID> groupsToRemove;
            struct GroupBackup {
                std::shared_ptr<UI::ProjectExplorerNode> node;
                UUID parentId;
            };
            std::vector<GroupBackup> backups;
            
        public:
            RemoveUIGroupsCommand(const std::vector<UUID>& groups) : groupsToRemove(groups) {}
            
            bool execute() override {
                auto &st = UI::ProjectExplorer::state;
                backups.clear();
                for (auto id : groupsToRemove) {
                    if (st.nodesLookup.contains(id)) {
                        auto node = st.nodesLookup[id];
                        backups.push_back({node, node->parentId});
                        st.removeNode(node);
                    }
                }
                return true;
            }
            
            std::any undo() override {
                auto &st = UI::ProjectExplorer::state;
                // Restore in reverse order
                for (auto it = backups.rbegin(); it != backups.rend(); ++it) {
                    st.addNode(it->node);
                    if (it->parentId != UUID::null && st.nodesLookup.contains(it->parentId)) {
                        st.moveNode(it->node, st.nodesLookup[it->parentId]);
                    }
                }
                return {};
            }
            
            std::any getResult() override { return {}; }
        };
        
        if (!groupsToRemove.empty()) {
            composite->addCommand(std::make_unique<RemoveUIGroupsCommand>(groupsToRemove));
        }

        m_internalCommand = std::move(composite);
        return m_internalCommand->execute();
    }

    std::any DeleteNodeCommand::undo() {
        if (m_internalCommand) {
            return m_internalCommand->undo();
        }
        return {};
    }

    std::any DeleteNodeCommand::getResult() {
        return {};
    }
}
