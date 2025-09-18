#pragma once

#include "bess_api.h"
#include "bess_uuid.h"
#include "command.h"
#include "component_types/component_types.h"
#include "entt_components.h"
#include "types.h"

#include "json/config.h"
#include "json/value.h"

namespace Bess::SimEngine::Commands {
    class BESS_API AddCommand : public Command {
      public:
        AddCommand(ComponentType type, int inputCount, int outputCount);
        bool execute() override;
        std::any undo() override;
        COMMAND_RESULT_OVERRIDE;

      private:
        ComponentType m_compType;
        int m_inputCount;
        int m_outputCount;
        UUID m_compId = UUID::null;
        Json::Value m_compJson;
        ConnectionBundle m_connections;

        bool m_redo = false;
    };

    class BESS_API ConnectCommand : public Command {
      public:
        ConnectCommand(const UUID &src, int srcPin, PinType srcType,
                       const UUID &dst, int dstPin, PinType dstType);
        bool execute() override;
        std::any undo() override;
        COMMAND_RESULT_OVERRIDE;

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
        DeleteCompCommand(const std::vector<UUID> &compId);
        bool execute() override;
        std::any undo() override;
        COMMAND_RESULT_OVERRIDE;

      private:
        struct DelCompData {
            UUID id;
            ConnectionBundle connections;
            Json::Value json;
        };

        std::vector<UUID> m_compIds;
        std::vector<DelCompData> m_delCompData;
    };

    class BESS_API DelConnectionCommand : public Command {
      public:
        DelConnectionCommand(const UUID &src, int srcPin, PinType srcType,
                             const UUID &dst, int dstPin, PinType dstType);
        bool execute() override;
        std::any undo() override;
        COMMAND_RESULT_OVERRIDE;

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
        std::any undo() override;
        COMMAND_RESULT_OVERRIDE;

      private:
        UUID m_compId;
        bool m_newState;
        bool m_oldState;
    };
} // namespace Bess::SimEngine::Commands
