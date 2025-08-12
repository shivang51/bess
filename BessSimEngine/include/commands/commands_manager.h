#pragma once
#include "command.h"
#include <memory>
#include <stack>

namespace Bess::SimEngine::Commands {
    class CommandsManager {
      public:
        static CommandsManager &instance();

        template <typename T, typename... Args>
        static void execute(Args &&...args) {
            std::unique_ptr<T> cmd = std::make_unique<T>(std::forward<Args>(args)...);

            if (!cmd->execute()) {
                return;
            }
            m_undoStack.push(std::move(cmd));
        }

        CommandsManager();

        void undo();
        void redo();

        void clearStacks();

      private:
        std::stack<std::unique_ptr<Command>> m_undoStack;
        std::stack<std::unique_ptr<Command>> m_redoStack;
    };
} // namespace Bess::SimEngine::Commands
