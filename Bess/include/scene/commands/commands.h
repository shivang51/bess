#pragma once

#include "bess_uuid.h"
#include "commands/command.h"
#include "vec2.hpp"

#include "component_definition.h"

namespace Bess::Canvas::Commands {
    using Command = SimEngine::Commands::Command;
    class AddCommand : public Command {
      public:
        AddCommand(
            std::shared_ptr<const SimEngine::ComponentDefinition> comp,
            const glm::vec2 &pos,
            int inpCount, int outCount);
        bool execute() override;
        void undo() override;
        RESULT_OVERRIDE;

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
        void undo() override;
        RESULT_OVERRIDE;

      private:
        entt::entity m_startSlot;
        entt::entity m_endSlot;
        entt::entity m_connection;
    };
} // namespace Bess::Canvas::Commands
