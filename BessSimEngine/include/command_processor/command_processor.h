#pragma once

#include "commands/commands_manager.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <expected>
#include <any>

namespace Bess::SimEngine {
    class CommandProcessor {
      public:
        static CommandProcessor &instance();

        using CommandCreationFunc = std::function<std::unique_ptr<Commands::Command>(const std::vector<std::string> &args)>;
        using CommandResult = std::expected<std::any, std::string>;

        /// Registers a command with its creation logic.
        void registerCommand(const std::string &name, CommandCreationFunc func);

        /// Processes a raw command string.
        /// returns some relevant value
        CommandResult process(const std::string &commandString);

      private:
        std::map<std::string, CommandCreationFunc> m_commandFactory;
        std::vector<std::string> tokenize(const std::string &str);
    };
} // namespace Bess::SimEngine