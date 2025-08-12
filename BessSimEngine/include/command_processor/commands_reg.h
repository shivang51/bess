#pragma once
#include "command_processor/command_processor.h"
#include "commands/commands.h"
#include "utils/string_utils.h"

namespace Bess::SimEngine::Commands {
    void registerAllCommands(CommandProcessor &processor) {
        processor.registerCommand("add", [](const std::vector<std::string> &args) -> std::unique_ptr<Bess::SimEngine::Commands::Command> {
            if (args.size() != 3)
                return nullptr;
            try {
                auto type = Bess::SimEngine::StringUtils::toComponentType(args[0]);
                if (type == Bess::SimEngine::ComponentType::EMPTY)
                    return nullptr;

                int inputCount = std::stoi(args[1]);
                int outputCount = std::stoi(args[2]);
                return std::make_unique<Bess::SimEngine::Commands::AddCommand>(type, inputCount, outputCount);
            } catch (const std::exception &e) {
                return nullptr;
            }
        });

        processor.registerCommand("conn", [](const std::vector<std::string> &args) {
            return nullptr;
        });
    }
} // namespace Bess::SimEngine::Commands