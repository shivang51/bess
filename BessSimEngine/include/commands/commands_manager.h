#pragma once
#include "bess_api.h"
#include "command.h"
#include <expected>
#include <memory>
#include <stack>

namespace Bess::SimEngine::Commands {
    class BESS_API CommandsManager {
      public:
        CommandsManager();

        template <typename T, typename RT, typename... Args>
        std::expected<RT, bool> execute(Args &&...args) {
            std::unique_ptr<T> cmd = std::make_unique<T>(std::forward<Args>(args)...);

            if (!cmd->execute()) {
                return std::unexpected(false);
            }
            auto res = cmd->template getResult<RT>();
            m_undoStack.push(std::move(cmd));
            return res;
        }

        template <typename T>
        bool execute(std::unique_ptr<T> cmd) {
            if (!cmd->execute()) {
                return false;
            }
            m_undoStack.push(std::move(cmd));
            return true;
        }

        std::any undo();
        void redo();

        void clearStacks();

        bool canUndo() const;
        bool canRedo() const;

      private:
        std::stack<std::unique_ptr<Command>> m_undoStack;
        std::stack<std::unique_ptr<Command>> m_redoStack;
    };
} // namespace Bess::SimEngine::Commands
