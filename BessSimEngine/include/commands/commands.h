#pragma once

#include "bess_api.h"
#include "bess_uuid.h"
#include "command.h"
#include "component_types/component_types.h"
#include "types.h"

namespace Bess::SimEngine::Commands {
    class BESS_API AddCommand : public Command {
      public:
        AddCommand(ComponentType type, int inputCount, int outputCount);
        bool execute() override;
        void undo() override;
        std::any getResult() override;
        using Command::getResult;

      private:
        ComponentType m_compType;
        int m_inputCount;
        int m_outputCount;
        UUID m_compId = UUID::null;
    };

    class BESS_API ConnectCommand : public Command {
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

    class BESS_API DeleteCompCommand : public Command {
      public:
        DeleteCompCommand(const UUID &compId);
        bool execute() override;
        void undo() override;
        std::any getResult() override;

      private:
        UUID m_compId;
        ComponentType m_compType = ComponentType::EMPTY;
        ConnectionBundle m_connections{};
    };

    class BESS_API DelConnectionCommand : public Command {
      public:
        DelConnectionCommand(const UUID &src, int srcPin, PinType srcType,
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

    class BESS_API SetInputCommand : public Command {
      public:
        SetInputCommand(const UUID &compId, bool state);
        bool execute() override;
        void undo() override;
        std::any getResult() override;

      private:
        UUID m_compId;
        bool m_newState;
        bool m_oldState;
    };
} // namespace Bess::SimEngine::Commands
