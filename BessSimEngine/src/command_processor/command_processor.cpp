#include "command_processor/command_processor.h"
#include "command_processor/commands_reg.h"
#include <logger.h>
#include <algorithm>
#include <sstream>

namespace Bess::SimEngine {
    CommandProcessor::CommandProcessor(Commands::CommandsManager &manager) : m_cmdManager(manager) {
        Commands::registerAllCommands(*this);
    }

    void CommandProcessor::registerCommand(const std::string &name, CommandCreationFunc func) {
        std::string lowerName = name;
        std::ranges::transform(lowerName, lowerName.begin(), ::tolower);
        m_commandFactory[lowerName] = std::move(func);
    }

    CommandProcessor::CommandResult CommandProcessor::process(const std::string &commandString) {
        if (commandString.empty())
            return std::unexpected("Empty Command");

        auto tokens = tokenize(commandString);
        if (tokens.empty())
            return std::unexpected("Invalid Command: " + commandString);


        std::string commandName = tokens[0];
        std::transform(commandName.begin(), commandName.end(), commandName.begin(), ::tolower);

        const auto it = m_commandFactory.find(commandName);
        if (it == m_commandFactory.end()) {
            BESS_SE_ERROR("[CommandProcessor] Unknown Command: {}", commandString);
            return std::unexpected("Unknown command: " + commandName);
        }

        const std::vector<std::string> args(tokens.begin() + 1, tokens.end());
        auto command = it->second(args);

        if (!command) {
			return std::unexpected("Command construction failed");
        }

        auto *cmd = command.get();
		m_cmdManager.execute(std::move(command));
        return cmd->getResult();
    }

    std::vector<std::string> CommandProcessor::tokenize(const std::string &str) {
        std::stringstream ss(str);
        std::string token;
        std::vector<std::string> tokens;
        while (ss >> token) {
            tokens.push_back(token);
        }
        return tokens;
    }
} // namespace Bess::SimEngine