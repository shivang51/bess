#pragma once
#include "commands/command.h"
#include "bess_uuid.h"
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
        // We need to store sub-commands for undo/redo
        std::unique_ptr<Command> m_internalCommand; 
    };
}

