#pragma once
#include "bess_api.h"
#include <any>

namespace Bess::SimEngine::Commands {
    class BESS_API Command {
      public:
        Command() = default;
        Command(Command &other) = default;

        /// Return false if failed
        virtual bool execute() = 0;

        virtual void undo() = 0;

        virtual std::any getResult() = 0;

        template <typename T>
        T getResult() {
            return std::any_cast<T>(getResult());
        }
    };

#define RESULT_OVERRIDE            \
    std::any getResult() override; \
    using Command::getResult;
} // namespace Bess::SimEngine::Commands
