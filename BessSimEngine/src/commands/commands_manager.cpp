#include "commands/commands_manager.h"

namespace Bess::SimEngine::Commands {
    CommandsManager::CommandsManager() {
        m_undoStack = {};
        m_redoStack = {};
    }

    std::any CommandsManager::undo() {
        if (m_undoStack.empty())
            return 0;

        auto cmd = std::move(m_undoStack.top());
        m_undoStack.pop();

        auto val = cmd->undo();
        m_redoStack.push(std::move(cmd));

        return val;
    }

    void CommandsManager::redo() {
        if (m_redoStack.empty())
            return;

        auto cmd = std::move(m_redoStack.top());
        m_redoStack.pop();

        if (cmd->execute())
            m_undoStack.push(std::move(cmd));
    }

    void CommandsManager::clearStacks() {
        m_undoStack = {};
        m_redoStack = {};
    }

    bool CommandsManager::canUndo() const {
        return !m_undoStack.empty();
    }

    bool CommandsManager::canRedo() const {
        return !m_redoStack.empty();
    }
} // namespace Bess::SimEngine::Commands
