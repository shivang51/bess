#include "commands/commands.h"
#include "simulation_engine.h"
#include "simulation_engine_serializer.h"

namespace Bess::SimEngine::Commands {
    AddCommand::AddCommand(ComponentType type, int inputCount, int outputCount) : m_compType(type), m_inputCount(inputCount), m_outputCount(outputCount) {
    }

    bool AddCommand::execute() {

        auto &engine = SimulationEngine::instance();

        if (m_redo) {
            SimEngineSerializer ser;
            ser.deserializeEntity(m_compJson);
            for (int i = 0; i < m_connections.inputs.size(); i++) {
                for (auto &conn : m_connections.inputs[i]) {
                    engine.connectComponent(m_compId, i, PinType::input, conn.first, conn.second, PinType::output, true);
                }
            }

            for (int i = 0; i < m_connections.outputs.size(); i++) {
                for (auto &conn : m_connections.outputs[i]) {
                    engine.connectComponent(m_compId, i, PinType::output, conn.first, conn.second, PinType::input, true);
                }
            }
        } else {
            m_compId = engine.addComponent(m_compType, m_inputCount, m_outputCount);
        }

        return m_compId != UUID::null;
    }

    std::any AddCommand::undo() {
        if (m_compId == UUID::null)
            return UUID::null;

        auto &engine = SimulationEngine::instance();

        m_compJson.clear();
        SimEngineSerializer ser;
        ser.serializeEntity(m_compId, m_compJson);

        m_connections = engine.getConnections(m_compId);

        engine.deleteComponent(m_compId);

        m_redo = true;

        return {};
    }

    std::any AddCommand::getResult() {
        return m_compId;
    }

    ConnectCommand::ConnectCommand(const UUID &src, int srcPin, PinType srcType, const UUID &dst, int dstPin, PinType dstType) {
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

    DeleteCompCommand::DeleteCompCommand(const UUID &compId) : m_compId(compId) {}

    bool DeleteCompCommand::execute() {
        if (m_compId == UUID::null)
            return false;

        auto &engine = SimulationEngine::instance();

        SimEngineSerializer ser;
        ser.serializeEntity(m_compId, m_compJson);

        m_connections = engine.getConnections(m_compId);

        engine.deleteComponent(m_compId);
        return true;
    }

    std::any DeleteCompCommand::undo() {
        if (m_compId == UUID::null)
            return UUID::null;

        SimEngineSerializer ser;
        ser.deserializeEntity(m_compJson);

        auto &engine = SimulationEngine::instance();

        for (int i = 0; i < m_connections.inputs.size(); i++) {
            for (auto &conn : m_connections.inputs[i]) {
                engine.connectComponent(m_compId, i, PinType::input, conn.first, conn.second, PinType::output, true);
            }
        }

        for (int i = 0; i < m_connections.outputs.size(); i++) {
            for (auto &conn : m_connections.outputs[i]) {
                engine.connectComponent(m_compId, i, PinType::output, conn.first, conn.second, PinType::input, true);
            }
        }

        return m_compId;
    }

    std::any DeleteCompCommand::getResult() {
        return std::string("deletion successful");
    }

    // --- Delete Connection Command ---
    DelConnectionCommand::DelConnectionCommand(const UUID &src, int srcPin, PinType srcType, const UUID &dst, int dstPin, PinType dstType)
        : m_src(src), m_srcPin(srcPin), m_srcType(srcType), m_dst(dst), m_dstPin(dstPin), m_dstType(dstType) {}

    bool DelConnectionCommand::execute() {
        SimulationEngine::instance().deleteConnection(m_src, m_srcType, m_srcPin, m_dst, m_dstType, m_dstPin);
        return true;
    }

    std::any DelConnectionCommand::undo() {
        SimulationEngine::instance().connectComponent(m_src, m_srcPin, m_srcType, m_dst, m_dstPin, m_dstType);

        return {};
    }

    std::any DelConnectionCommand::getResult() {
        return std::string("deletion successful");
    }

    // --- SetInputCommand ---
    SetInputCommand::SetInputCommand(const UUID &compId, bool state)
        : m_compId(compId), m_newState(state), m_oldState(false) {}

    bool SetInputCommand::execute() {
        auto &engine = SimulationEngine::instance();
        m_oldState = engine.getDigitalPinState(m_compId, PinType::output, 0);

        if (m_oldState == m_newState) {
            return false;
        }

        engine.setDigitalInput(m_compId, m_newState);
        return true;
    }

    std::any SetInputCommand::undo() {
        SimulationEngine::instance().setDigitalInput(m_compId, m_oldState);

        return {};
    }

    std::any SetInputCommand::getResult() {
        return m_newState;
    }
} // namespace Bess::SimEngine::Commands
