#pragma once
#include "bess_uuid.h"
#include "commands/command.h"
#include "vec2.hpp"

#include "component_definition.h"
#include "scene/components/components.h"
#include "json/json.h"
#include "json/value.h"

namespace Bess::Canvas::Commands {
    using Command = SimEngine::Commands::Command;

    class DeleteCompCommand : public Command {
      public:
        DeleteCompCommand(const std::vector<UUID> &compIds);

        bool execute() override;
        std::any undo() override;
        COMMAND_RESULT_OVERRIDE;

      private:
        struct DeleteCompCommandData {
            UUID id, simCompId = UUID::null;
            std::unordered_map<UUID, std::vector<UUID>> connections;
            Json::Value compJson;
        };
        std::vector<UUID> m_compIds;
        std::vector<DeleteCompCommandData> m_delData;
        std::vector<UUID> m_simEngineComps = {};
        bool m_redo = false;
    };
} // namespace Bess::Canvas::Commands
