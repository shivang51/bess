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
} // namespace Bess::Canvas::Commands
