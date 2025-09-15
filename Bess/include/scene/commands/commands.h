#pragma once

#include "bess_uuid.h"
#include "commands/command.h"
#include "vec2.hpp"

#include "component_definition.h"
#include "scene/components/components.h"

namespace Bess::Canvas::Commands {
    using Command = SimEngine::Commands::Command;
    class AddCommand : public Command {
      public:
        AddCommand(
            std::shared_ptr<const SimEngine::ComponentDefinition> comp,
            const glm::vec2 &pos,
            int inpCount, int outCount);
        bool execute() override;
        std::any undo() override;
        COMMAND_RESULT_OVERRIDE;

      private:
        UUID m_compId = UUID::null;
        glm::vec2 m_lastPos;
        std::shared_ptr<const SimEngine::ComponentDefinition> m_compDef;
        int m_inpCount;
        int m_outCount;
    };

    class ConnectCommand : public Command {
      public:
        ConnectCommand(entt::entity startSlot, entt::entity endSlot);
        bool execute() override;
        std::any undo() override;
        COMMAND_RESULT_OVERRIDE;

      private:
        entt::entity m_startSlot;
        entt::entity m_endSlot;
        entt::entity m_connection;
    };

    class BESS_API DeleteCompCommand : public Command {
      public:
        DeleteCompCommand(const UUID &compId);
        bool execute() override;
        std::any undo() override;
        COMMAND_RESULT_OVERRIDE;

      private:
        UUID m_compId;
        Json::Value m_compJson;
        bool m_isSimComponent = false;
        std::unordered_map<UUID, std::vector<UUID>> m_connections;
    };
} // namespace Bess::Canvas::Commands
