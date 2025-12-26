#pragma once

#include "bess_api.h"
#include "bess_uuid.h"
#include "command.h"
#include "entt_components.h"
#include "types.h"

#include "json/config.h"
#include "json/value.h"
#include <cstdint>

namespace Bess::SimEngine::Commands {
    struct BESS_API AddCommandData {
        std::shared_ptr<ComponentDefinition> def;
        int inputCount;
        int outputCount;
    };

    class BESS_API AddCommand : public Command {
      public:
        AddCommand(const std::vector<AddCommandData> &data);
        bool execute() override;
        std::any undo() override;
        COMMAND_RESULT_OVERRIDE;

      private:
        std::vector<AddCommandData> m_data;
        std::vector<UUID> m_compIds = {};
        std::vector<Json::Value> m_compJsons = {};
        std::vector<ConnectionBundle> m_connections;

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

    struct BESS_API DelConnectionCommandData {
        UUID src;
        uint32_t srcPin;
        PinType srcType;
        UUID dst;
        uint32_t dstPin;
        PinType dstType;
    };

    class BESS_API DelConnectionCommand : public Command {
      public:
        DelConnectionCommand(const std::vector<DelConnectionCommandData> &delData);
        bool execute() override;
        std::any undo() override;
        COMMAND_RESULT_OVERRIDE;

      private:
        std::vector<DelConnectionCommandData> m_delData = {};
    };

    class BESS_API SetInputCommand : public Command {
      public:
        SetInputCommand(const UUID &compId, LogicState state);
        bool execute() override;
        std::any undo() override;
        COMMAND_RESULT_OVERRIDE;

      private:
        UUID m_compId;
        LogicState m_newState;
        LogicState m_oldState;
    };
} // namespace Bess::SimEngine::Commands
