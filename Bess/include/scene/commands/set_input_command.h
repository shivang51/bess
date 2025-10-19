#pragma once
#include "commands/commands.h"
#include "types.h"

namespace Bess::Canvas::Commands {
    class SetInputCommand : public SimEngine::Commands::Command {
      public:
        SetInputCommand(const UUID &compId, SimEngine::LogicState state);
        bool execute() override;
        std::any undo() override;
        COMMAND_RESULT_OVERRIDE;

      private:
        UUID m_compId;
        SimEngine::LogicState m_state;
        bool m_redo = false;
    };
} // namespace Bess::Canvas::Commands
