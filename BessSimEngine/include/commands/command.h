#pragma once
#include <any>

namespace Bess::SimEngine::Commands{
    class Command {
        public:
		Command() = default;
        Command(Command &other) = default;

        /// Return false if failed 
        virtual bool execute() = 0;
        
        virtual void undo() = 0;

        virtual std::any getResult() = 0;
    };
}