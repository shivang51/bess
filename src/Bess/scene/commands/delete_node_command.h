#pragma once
#include "bess_uuid.h"
#include "commands/command.h"
#include <vector>

namespace Bess::Canvas::Commands {
    using Command = SimEngine::Commands::Command;

    class DeleteNodeCommand : public Command {
      public:
        DeleteNodeCommand(const std::vector<UUID> &nodeIds);

        bool execute() override;
        std::any undo() override;
        COMMAND_RESULT_OVERRIDE;

      private:
        std::vector<UUID> m_nodeIds;
    };
} // namespace Bess::Canvas::Commands
