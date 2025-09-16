#pragma once
#include "bess_uuid.h"
#include "commands/command.h"
#include "vec2.hpp"

#include "component_definition.h"
#include "scene/components/components.h"
#include "json/json.h"

namespace Bess::Canvas::Commands {
    using Command = SimEngine::Commands::Command;
    class BESS_API DeleteCompCommand : public Command {
      public:
        DeleteCompCommand(const UUID &compId);
        bool execute() override;
        std::any undo() override;
        COMMAND_RESULT_OVERRIDE;

      private:
        UUID m_compId;
        Json::Value m_compJson;
        bool m_isSimComponent = false, m_redo = false;
        std::unordered_map<UUID, std::vector<UUID>> m_connections;
    };
} // namespace Bess::Canvas::Commands
