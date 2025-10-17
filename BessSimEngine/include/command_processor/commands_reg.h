#pragma once
#include "command_processor/command_processor.h"
#include "commands/commands.h"
#include "utils/string_utils.h"
#include <cstdint>

namespace Bess::SimEngine::Commands {
    void registerAllCommands(CommandProcessor &processor) {
        processor.registerCommand("add", [](const std::vector<std::string> &args) -> std::unique_ptr<Bess::SimEngine::Commands::Command> {
            if (args.size() != 3)
                return nullptr;
            try {
                // auto type = Bess::SimEngine::StringUtils::toComponentType(args[0]);
                // if (type == Bess::SimEngine::ComponentType::EMPTY)
                //     return nullptr;
                //
                return nullptr;

                int inputCount = std::stoi(args[1]);
                int outputCount = std::stoi(args[2]);
                // SimEngine::Commands::AddCommandData data = {type, inputCount, outputCount};
                // return std::make_unique<Bess::SimEngine::Commands::AddCommand>(std::vector{data});
            } catch (const std::exception &e) {
                return nullptr;
            }
        });

        // --- DELETE COMPONENT ---
        processor.registerCommand("del-comp", [](const std::vector<std::string> &args) -> std::unique_ptr<Command> {
            if (args.size() != 1)
                return nullptr;
            try {
                std::vector<UUID> id = {std::stoull(args[0])};
                return std::make_unique<DeleteCompCommand>(id);
            } catch (const std::exception &) {
                return nullptr;
            }
        });

        // --- SET INPUT STATE ---
        processor.registerCommand("set-input", [](const std::vector<std::string> &args) -> std::unique_ptr<Command> {
            if (args.size() != 2)
                return nullptr;
            try {
                UUID id(std::stoull(args[0]));
                bool state = std::stoi(args[1]);
                return std::make_unique<SetInputCommand>(id, state);
            } catch (const std::exception &) {
                return nullptr;
            }
        });

        // --- CONNECT PINS ---
        auto connectFunc = [](const std::vector<std::string> &args) -> std::unique_ptr<Command> {
            if (args.size() != 6)
                return nullptr;
            try {
                UUID srcId(std::stoull(args[0]));
                int srcPin = std::stoi(args[1]);
                PinType srcType = StringUtils::toPinType(args[2]);
                UUID dstId(std::stoull(args[3]));
                int dstPin = std::stoi(args[4]);
                PinType dstType = StringUtils::toPinType(args[5]);
                return std::make_unique<ConnectCommand>(srcId, srcPin, srcType, dstId, dstPin, dstType);
            } catch (const std::exception &) {
                return nullptr;
            }
        };
        processor.registerCommand("conn", connectFunc);

        // --- DISCONNECT PINS ---
        processor.registerCommand("del-conn", [](const std::vector<std::string> &args) -> std::unique_ptr<Command> {
            if (args.size() != 6)
                return nullptr;
            try {
                UUID srcId(std::stoull(args[0]));
                uint32_t srcPin = std::stoul(args[1]);
                PinType srcType = StringUtils::toPinType(args[2]);
                UUID dstId(std::stoull(args[3]));
                uint32_t dstPin = std::stoul(args[4]);
                PinType dstType = StringUtils::toPinType(args[5]);
                DelConnectionCommandData data = {srcId, srcPin, srcType, dstId, dstPin, dstType};
                return std::make_unique<DelConnectionCommand>(std::vector{data});
            } catch (const std::exception &) {
                return nullptr;
            }
        });
    }
} // namespace Bess::SimEngine::Commands
