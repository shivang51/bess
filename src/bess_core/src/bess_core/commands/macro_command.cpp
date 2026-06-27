#include "bess_core/commands/macro_command.h"

#include <algorithm>

namespace Bess::Cmd {
    MacroCommand::MacroCommand() {
        m_name = "MacroCommand";
    }

    MacroCommand::MacroCommand(std::vector<std::unique_ptr<Command>> commands)
        : m_commands(std::move(commands)) {
        m_name = "MacroCommand";
    }

    bool MacroCommand::execute(const CommandContext &context) {
        const auto commandContext = makeContext(context);
        m_executedCount = 0;

        for (const auto &cmd : m_commands) {
            if (!cmd) {
                continue;
            }

            if (!cmd->execute(commandContext)) {
                for (size_t idx = m_executedCount; idx > 0; --idx) {
                    if (m_commands[idx - 1]) {
                        m_commands[idx - 1]->undo(commandContext);
                    }
                }
                m_executedCount = 0;
                return false;
            }

            ++m_executedCount;
        }

        return true;
    }

    void MacroCommand::undo(const CommandContext &context) {
        const auto commandContext = makeContext(context);
        const auto count = std::min(m_executedCount, m_commands.size());
        for (size_t idx = count; idx > 0; --idx) {
            if (m_commands[idx - 1]) {
                m_commands[idx - 1]->undo(commandContext);
            }
        }
    }

    void MacroCommand::redo(const CommandContext &context) {
        const auto commandContext = makeContext(context);
        for (const auto &cmd : m_commands) {
            if (cmd) {
                cmd->redo(commandContext);
            }
        }
        m_executedCount = m_commands.size();
    }

    void MacroCommand::addCommand(std::unique_ptr<Command> cmd) {
        if (cmd) {
            m_commands.push_back(std::move(cmd));
        }
    }

    bool MacroCommand::empty() const {
        return m_commands.empty();
    }

    size_t MacroCommand::size() const {
        return m_commands.size();
    }
} // namespace Bess::Cmd
