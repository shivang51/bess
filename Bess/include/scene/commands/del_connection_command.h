#pragma once
#include "bess_uuid.h"
#include "commands/command.h"
#include "json/value.h"

namespace Bess::Canvas::Commands {
    class DelConnectionCommand : public SimEngine::Commands::Command {
      public:
        DelConnectionCommand(UUID uuid);
        bool execute() override;
        std::any undo() override;
        COMMAND_RESULT_OVERRIDE;

      private:
        UUID m_uuid;
        Json::Value m_json;
        bool m_redo = false;
    };
} // namespace Bess::Canvas::Commands
