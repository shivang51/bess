#pragma once
#include "bess_uuid.h"
#include "commands/command.h"
#include "json/value.h"

namespace Bess::Canvas::Commands {
    class DelConnectionCommand : public SimEngine::Commands::Command {
      public:
        DelConnectionCommand(const std::vector<UUID> &uuid);
        bool execute() override;
        std::any undo() override;
        COMMAND_RESULT_OVERRIDE;

      private:
        std::vector<UUID> m_uuids;
        std::vector<Json::Value> m_jsons;
        bool m_redo = false;
    };
} // namespace Bess::Canvas::Commands
