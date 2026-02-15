#pragma once
#include "application/app_types.h"
#include "commands/commands.h"

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
