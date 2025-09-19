#pragma once
#include "bess_uuid.h"
#include "commands/command.h"
#include "vec2.hpp"

#include "component_definition.h"
#include "scene/components/components.h"
#include "json/value.h"

namespace Bess::Canvas::Commands {
    using Command = SimEngine::Commands::Command;

    struct AddCommandData {
        std::shared_ptr<const SimEngine::ComponentDefinition> def = nullptr;
        Components::NSComponent nsComp;
        int inputCount, outputCount;
        glm::vec2 pos;
        bool isSimComp() const {
            return def != nullptr;
        }
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
