#pragma once

#include "bess_core/commands/command.h"
#include <memory>
#include <vector>

namespace Bess::Cmd {
    class BESS_API MacroCommand : public Command {
      public:
        MacroCommand();
        explicit MacroCommand(std::vector<std::unique_ptr<Command>> commands);

        bool execute(const CommandContext &context) override;
        void undo(const CommandContext &context) override;
        void redo(const CommandContext &context) override;

        void addCommand(std::unique_ptr<Command> cmd);
        bool empty() const;
        size_t size() const;

      private:
        std::vector<std::unique_ptr<Command>> m_commands;
        size_t m_executedCount = 0;
    };
} // namespace Bess::Cmd
