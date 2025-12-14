#pragma once
#include "bess_uuid.h"
#include "commands/command.h"
#include <string>

namespace Bess::Canvas::Commands {
    using Command = SimEngine::Commands::Command;

    class ReparentNodeCommand : public Command {
      public:
        ReparentNodeCommand(const UUID &entityId, const UUID &newParentId);

        bool execute() override;
        std::any undo() override;
        COMMAND_RESULT_OVERRIDE;

      private:
        UUID m_entityId;
        UUID m_newParentId;
        UUID m_oldParentId = UUID::null;
        bool m_firstRun = true;
    };
} // namespace Bess::Canvas::Commands


