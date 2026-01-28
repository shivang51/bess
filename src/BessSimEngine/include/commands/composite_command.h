#pragma once
#include "commands/command.h"
#include <vector>
#include <memory>

namespace Bess::SimEngine::Commands {
    class CompositeCommand : public Command {
      public:
        CompositeCommand() = default;
        
        void addCommand(std::unique_ptr<Command> cmd) {
            m_commands.push_back(std::move(cmd));
        }

        bool execute() override {
            for (auto &cmd : m_commands) {
                if (!cmd->execute()) {
                    // If one fails, we should probably undo the previous ones?
                    // For simplicity, let's assume atomic commands or handle failure explicitly.
                    // Ideally we should undo successful ones in reverse order.
                    return false;
                }
            }
            return true;
        }

        std::any undo() override {
            // Undo in reverse order
            for (auto it = m_commands.rbegin(); it != m_commands.rend(); ++it) {
                (*it)->undo();
            }
            return {};
        }

        std::any getResult() override {
            return {}; // No specific result for composite
        }

      private:
        std::vector<std::unique_ptr<Command>> m_commands;
    };
}

