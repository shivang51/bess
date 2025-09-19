#pragma once
#include "commands/commands.h"

namespace Bess::Canvas::Commands {
    class SetInputCommand : public SimEngine::Commands::Command {
      public:
        SetInputCommand(const UUID &compId, bool state);
        bool execute() override;
        std::any undo() override;
        COMMAND_RESULT_OVERRIDE;

      private:
        UUID m_compId;
        bool m_state, m_redo = false;
    };
} // namespace Bess::Canvas::Commands
