#pragma once
#include "commands/commands.h"

namespace Bess::Canvas::Commands {
    class DelConnectionCommand : public SimEngine::Commands::DelConnectionCommand {
      public:
        DelConnectionCommand(const UUID &src, int srcPin, SimEngine::PinType srcType,
                             const UUID &dst, int dstPin, SimEngine::PinType dstType);
        bool execute() override;
        std::any undo() override;
        COMMAND_RESULT_OVERRIDE;
    };
} // namespace Bess::Canvas::Commands
