#pragma once

#include "command.h"
#include "component_types/component_types.h"
#include "types.h"
#include "bess_uuid.h"

namespace Bess::SimEngine::Commands {
    class AddCommand : public Command {
      public:
        AddCommand(ComponentType type, int inputCount, int outputCount);
        bool execute() override;
        void undo() override;
        std::any getResult() override;

      private:
        ComponentType m_compType;
        int m_inputCount;
        int m_outputCount;
        UUID m_compId = UUID::null;
    };

    class ConnectCommand : public Command {
      public:
        ConnectCommand(const UUID &src, int srcPin, PinType srcType,
                       const UUID &dst, int dstPin, PinType dstType);
        bool execute() override;
        void undo() override;
        std::any getResult() override;

      private:
        UUID m_src;
        int m_srcPin;
        PinType m_srcType;
        UUID m_dst;
        int m_dstPin;
        PinType m_dstType;
    };
} // namespace Bess::SimEngine::Commands
