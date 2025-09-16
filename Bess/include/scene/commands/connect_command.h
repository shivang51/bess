#pragma once
#include "bess_uuid.h"
#include "commands/command.h"
#include "vec2.hpp"

#include "component_definition.h"
#include "scene/components/components.h"

namespace Bess::Canvas::Commands {
    using Command = SimEngine::Commands::Command;
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
} // namespace Bess::Canvas::Commands
