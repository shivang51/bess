#include "commands/commands.h"
#include "common/bess_uuid.h"
#include "simulation_engine.h"
#include "simulation_engine_serializer.h"
#include "types.h"
#include "json/value.h"

namespace Bess::SimEngine::Commands {
    AddCommand::AddCommand(const std::vector<AddCommandData> &data) : m_data(data) {
        m_compJsons = std::vector<Json::Value>(m_data.size(), Json::objectValue);
        m_compIds = std::vector(m_data.size(), UUID::null);
        m_connections = std::vector<ConnectionBundle>(m_data.size());
    }

    bool AddCommand::execute() {
        auto &engine = SimulationEngine::instance();

        SimEngineSerializer ser;
        for (int i = 0; i < m_data.size(); i++) {
            auto &compJson = m_compJsons[i];
            auto &connections = m_connections[i];
            auto &compId = m_compIds[i];
            if (m_redo) {
                ser.deserializeEntity(compJson);
                for (int i = 0; i < connections.inputs.size(); i++) {
                    for (auto &conn : connections.inputs[i]) {
                        engine.connectComponent(compId, i, SlotType::digitalInput, conn.first, conn.second, SlotType::digitalOutput, true);
                    }
                }

                for (int i = 0; i < connections.outputs.size(); i++) {
                    for (auto &conn : connections.outputs[i]) {
                        engine.connectComponent(compId, i, SlotType::digitalOutput, conn.first, conn.second, SlotType::digitalInput, true);
                    }
                }
            } else {
                const auto &data = m_data[i];
                compId = engine.addComponent(data.def);
            }
        }

        return !m_compIds.empty();
    }

    std::any AddCommand::undo() {
        auto &engine = SimulationEngine::instance();

        for (int i = 0; i < m_data.size(); i++) {
            auto &compJson = m_compJsons[i];
            auto &connections = m_connections[i];
            auto &compId = m_compIds[i];
            if (compId == UUID::null)
                continue;

            compJson.clear();
            SimEngineSerializer ser;
            ser.serializeEntity(compId, compJson);

            connections = engine.getConnections(compId);

            engine.deleteComponent(compId);
        }

        m_redo = true;

        return {};
    }

    std::any AddCommand::getResult() {
        return m_compIds;
    }

    ConnectCommand::ConnectCommand(const UUID &src, int srcPin, SlotType srcType, const UUID &dst, int dstPin, SlotType dstType) {
        m_src = src;
        m_srcPin = srcPin;
        m_srcType = srcType;
        m_dst = dst;
        m_dstPin = dstPin;
        m_dstType = dstType;
    }

    bool ConnectCommand::execute() {
        return SimulationEngine::instance().connectComponent(m_src, m_srcPin, m_srcType, m_dst, m_dstPin, m_dstType);
    }

    std::any ConnectCommand::undo() {
        SimulationEngine::instance().deleteConnection(m_src, m_srcType, m_srcPin, m_dst, m_dstType, m_dstPin);
        return {};
    }

    std::any ConnectCommand::getResult() {
        return std::string("successfully connected");
    }

    DeleteCompCommand::DeleteCompCommand(const std::vector<UUID> &compIds) : m_compIds(compIds) {
        m_delCompData = std::vector<DelCompData>(m_compIds.size(), {UUID::null, {}, Json::objectValue});
    }

    bool DeleteCompCommand::execute() {
        auto &engine = SimulationEngine::instance();
        SimEngineSerializer ser;
        size_t i = 0;
        for (const auto compId : m_compIds) {
            auto &data = m_delCompData[i++];
            data.id = compId;
            data.json.clear();
            ser.serializeEntity(compId, data.json);
            data.connections = engine.getConnections(compId);
            engine.deleteComponent(compId);
        }

        return true;
    }

    std::any DeleteCompCommand::undo() {
        SimEngineSerializer ser;
        for (const auto &delData : m_delCompData) {
            ser.deserializeEntity(delData.json);

            auto &engine = SimulationEngine::instance();

            const auto compId = delData.id;
            const auto &connections = delData.connections;

            for (int i = 0; i < connections.inputs.size(); i++) {
                for (auto &conn : connections.inputs[i]) {
                    engine.connectComponent(compId, i, SlotType::digitalInput, conn.first, conn.second, SlotType::digitalOutput, true);
                }
            }

            for (int i = 0; i < connections.outputs.size(); i++) {
                for (auto &conn : connections.outputs[i]) {
                    engine.connectComponent(compId, i, SlotType::digitalOutput, conn.first, conn.second, SlotType::digitalInput, true);
                }
            }
        }

        return true;
    }

    std::any DeleteCompCommand::getResult() {
        return std::string("deletion successful");
    }

    // --- Delete Connection Command ---
    DelConnectionCommand::DelConnectionCommand(const std::vector<DelConnectionCommandData> &data)
        : m_delData(data) {}

    bool DelConnectionCommand::execute() {
        auto &engine = SimulationEngine::instance();
        for (auto &data : m_delData) {
            engine.deleteConnection(data.src, data.srcType, data.srcPin,
                                    data.dst, data.dstType, data.dstPin);
        }
        return true;
    }

    std::any DelConnectionCommand::undo() {
        auto &engine = SimulationEngine::instance();
        for (auto &data : m_delData) {
            engine.connectComponent(data.src, data.srcPin, data.srcType, data.dst, data.dstPin, data.dstType);
        }

        return {};
    }

    std::any DelConnectionCommand::getResult() {
        return std::string("deletion successful");
    }

    // --- SetInputCommand ---
    SetInputCommand::SetInputCommand(const UUID &compId, LogicState state)
        : m_compId(compId), m_newState(state) {
    }

    bool SetInputCommand::execute() {
        auto &engine = SimulationEngine::instance();
        m_oldState = engine.getDigitalSlotState(m_compId, SlotType::digitalOutput, 0).state;

        if (m_oldState == m_newState) {
            return false;
        }

        engine.setOutputSlotState(m_compId, 0,
                                  m_newState == LogicState::low ? LogicState::low : LogicState::high);
        return true;
    }

    std::any SetInputCommand::undo() {
        SimulationEngine::instance().setOutputSlotState(m_compId, 0,
                                                        m_oldState == LogicState::low ? LogicState::low : LogicState::high);

        return {};
    }

    std::any SetInputCommand::getResult() {
        return m_newState == LogicState::high;
    }
} // namespace Bess::SimEngine::Commands
