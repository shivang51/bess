#pragma once
#include "bess_api.h"
#include "command.h"
#include <memory>
#include <stack>

namespace Bess::SimEngine::Commands {
    class BESS_API CommandsManager {
      public:
        CommandsManager();

        template <typename T, typename... Args>
        void execute(Args &&...args) {
            std::unique_ptr<T> cmd = std::make_unique<T>(std::forward<Args>(args)...);

            if (!cmd->execute()) {
                return;
            }
            m_undoStack.push(std::move(cmd));
        }

        template <typename T>
        void execute(std::unique_ptr<T> cmd) {
            if (!cmd->execute()) {
                return;
            }
            m_undoStack.push(std::move(cmd));
        }

        void undo();
        void redo();

        void clearStacks();

      private:
        std::stack<std::unique_ptr<Command>> m_undoStack;
        std::stack<std::unique_ptr<Command>> m_redoStack;
    };
} // namespace Bess::SimEngine::Commands
