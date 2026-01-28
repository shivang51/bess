#pragma once
#include "bess_api.h"
#include <any>
#include <format>
#include <stdexcept>

namespace Bess::SimEngine::Commands {
    class BESS_API Command {
      public:
        Command() = default;
        Command(Command &other) = default;

        virtual ~Command() = default;

        /// Return false if failed
        virtual bool execute() = 0;

        virtual std::any undo() = 0;

        virtual std::any getResult() = 0;

        template <typename T>
        T getResult() {
            if (getResult().type() != typeid(T)) {
                throw std::runtime_error(std::format("Command result type mismatch, req cast type: {}, got type: {}",
                                                     typeid(T).name(), getResult().type().name()));
            }
            return std::any_cast<T>(getResult());
        }
    };

#define COMMAND_RESULT_OVERRIDE    \
    std::any getResult() override; \
    using Command::getResult;
} // namespace Bess::SimEngine::Commands
