#pragma once
#include "commands/commands.h"

namespace Bess::Canvas::Commands {
    class SetInputCommand : public SimEngine::Commands::SetInputCommand {
      public:
        SetInputCommand(const UUID &compId, bool state);
        bool execute() override;
        std::any undo() override;
        COMMAND_RESULT_OVERRIDE;
    };
} // namespace Bess::Canvas::Commands
