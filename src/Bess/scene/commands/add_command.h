#pragma once
#include "bess_uuid.h"
#include "commands/command.h"
#include "scene/components/non_sim_comp.h"

#include "component_definition.h"
#include "json/value.h"

namespace Bess::Canvas::Commands {
    using Command = SimEngine::Commands::Command;

    struct AddCommandData {
        std::shared_ptr<SimEngine::ComponentDefinition> def;
        Components::NSComponent nsComp;
        size_t inputCount = -1, outputCount = -1;
        glm::vec2 pos;
    };

    class AddCommand : public Command {
      public:
        AddCommand(const std::vector<AddCommandData> &data);

        bool execute() override;
        std::any undo() override;
        COMMAND_RESULT_OVERRIDE;

      private:
        std::vector<AddCommandData> m_data;
        std::vector<UUID> m_compIds = {};
        std::vector<Json::Value> m_compJsons = {};
        bool m_redo = false;
    };
} // namespace Bess::Canvas::Commands
