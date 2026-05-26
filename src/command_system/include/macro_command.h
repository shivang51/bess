#pragma once
#include "command.h"
#include <memory>
#include <vector>

namespace Bess::Cmd {
    class MacroCommand : public Command {
      public:
        MacroCommand() { m_name = "MacroCommand"; }

        MacroCommand(std::vector<std::unique_ptr<Command>> commands)
            : m_commands(std::move(commands)) {
            m_name = "MacroCommand";
        }

        bool execute(
            std::shared_ptr<Canvas::Scene> &scene,
            std::shared_ptr<SimEngine::SimulationEngine> &simEngine) override;

        void
        undo(std::shared_ptr<Canvas::Scene> &scene,
             std::shared_ptr<SimEngine::SimulationEngine> &simEngine) override;

        void
        redo(std::shared_ptr<Canvas::Scene> &scene,
             std::shared_ptr<SimEngine::SimulationEngine> &simEngine) override;

        void addCommand(std::unique_ptr<Command> cmd);

      private:
        std::vector<std::unique_ptr<Command>> m_commands;
    };
} // namespace Bess::Cmd
