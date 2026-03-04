#include "macro_command.h"
#include <memory>
#include <ranges>

namespace Bess::Cmd {
    bool MacroCommand::execute(Canvas::Scene *scene,
                               SimEngine::SimulationEngine *simEngine) {
        for (const auto &cmd : m_commands) {
            if (!cmd->execute(scene, simEngine)) {
                return false;
            }
        }
        return true;
    }

    void MacroCommand::undo(Canvas::Scene *scene,
                            SimEngine::SimulationEngine *simEngine) {
        for (auto &m_command : std::ranges::reverse_view(m_commands)) {
            m_command->undo(scene, simEngine);
        }
    }

    void MacroCommand::redo(Canvas::Scene *scene,
                            SimEngine::SimulationEngine *simEngine) {
        for (const auto &cmd : m_commands) {
            cmd->redo(scene, simEngine);
        }
    }

    void MacroCommand::addCommand(std::unique_ptr<Command> cmd) {
        m_commands.push_back(std::move(cmd));
    }
} // namespace Bess::Cmd
