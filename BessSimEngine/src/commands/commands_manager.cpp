#include "commands/commands_manager.h"
#include "command_processor/command_processor.h"

namespace Bess::SimEngine::Commands {
    CommandsManager &CommandsManager::instance() {
        static CommandsManager manager;
        return manager;
    }

    CommandsManager::CommandsManager() {
        m_undoStack = {};
        m_redoStack = {};
    }

    void CommandsManager::undo() {
        if (m_undoStack.empty())
            return;

        auto cmd = std::move(m_undoStack.top());
        m_undoStack.pop();

        cmd->undo();
        m_redoStack.push(std::move(cmd));
    }

    void CommandsManager::redo() {
        if (m_redoStack.empty())
            return;

        auto cmd = std::move(m_redoStack.top());
        m_redoStack.pop();

        if(cmd->execute())
			m_undoStack.push(std::move(cmd));
    }

    void CommandsManager::clearStacks() {
        m_undoStack = {};
        m_redoStack = {};
    }
} // namespace Bess::SimEngine::Commands
