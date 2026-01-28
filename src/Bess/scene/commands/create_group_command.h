#pragma once
#include "commands/command.h"
#include "bess_uuid.h"
#include <string>

namespace Bess::Canvas::Commands {
    using Command = SimEngine::Commands::Command;

    class CreateGroupCommand : public Command {
      public:
        CreateGroupCommand(const std::string &name);
        
        bool execute() override;
        std::any undo() override;
        COMMAND_RESULT_OVERRIDE;

      private:
        std::string m_name;
        UUID m_groupId {UUID::null};
        UUID m_parentUUID = UUID::null; // If we want to support creating inside another group later
    };
}

