#pragma once
#include "bess_uuid.h"
#include "commands/command.h"

#include "json/value.h"
#include "json/writer.h"

namespace Bess::Canvas::Commands {
    using Command = SimEngine::Commands::Command;
    class ConnectCommand : public Command {
      public:
        ConnectCommand(UUID startSlot, UUID endSlot);
        bool execute() override;
        std::any undo() override;
        COMMAND_RESULT_OVERRIDE;

      private:
        UUID m_startSlot;
        UUID m_endSlot;
        UUID m_connection;
        Json::Value m_json;

        bool m_redo = false;
        bool m_delete = false;
    };
} // namespace Bess::Canvas::Commands
