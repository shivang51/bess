#include "scene/commands/create_group_command.h"
#include "ui/ui_main/project_explorer.h"

namespace Bess::Canvas::Commands {
    CreateGroupCommand::CreateGroupCommand(const std::string &name)
        : m_name(name) {}

    bool CreateGroupCommand::execute() {
        auto &state = UI::ProjectExplorer::state;

        auto groupNode = std::make_shared<UI::ProjectExplorerNode>();
        groupNode->isGroup = true;
        groupNode->label = m_name;

        state.addNode(groupNode);
        m_groupId = groupNode->nodeId;

        return true;
    }

    std::any CreateGroupCommand::undo() {
        auto &state = UI::ProjectExplorer::state;
        auto node = state.nodesLookup[m_groupId];
        if (node) {
            state.removeNode(node);
        }
        return {};
    }

    std::any CreateGroupCommand::getResult() {
        return m_groupId;
    }
} // namespace Bess::Canvas::Commands
