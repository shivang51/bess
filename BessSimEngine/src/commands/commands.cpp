#include "commands/commands.h"
#include "simulation_engine.h"

namespace Bess::SimEngine::Commands {
    AddCommand::AddCommand(ComponentType type, int inputCount, int outputCount) : 
        m_compType(type), m_inputCount(inputCount), m_outputCount(outputCount) {
    }

    bool AddCommand::execute() {
        m_compId = SimulationEngine::instance().addComponent(m_compType, m_inputCount, m_outputCount);
        return m_compId != UUID::null;
    }

    void AddCommand::undo() {
        if (m_compId == UUID::null)
            return;

        SimulationEngine::instance().deleteComponent(m_compId);
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

    void ConnectCommand::undo() {
        SimulationEngine::instance().deleteConnection(m_src, m_srcType, m_srcPin, m_dst, m_dstType, m_dstPin);
    }

    std::any ConnectCommand::getResult() {
        return "successfully connected";
    }

	DeleteCompCommand::DeleteCompCommand(const UUID &compId) : m_compId(compId) {}

    bool DeleteCompCommand::execute() {
        if (m_compId == UUID::null)
            return false;

        auto &engine = SimulationEngine::instance();
        m_compType = engine.getComponentType(m_compId);
        m_connections = engine.getConnections(m_compId);

        engine.deleteComponent(m_compId);
        return true;
    }

    void DeleteCompCommand::undo() {
        if (m_compId == UUID::null)
            return;

        auto &engine = SimulationEngine::instance();
        const auto &newId = engine.addComponent(m_compType);
    }

    std::any DeleteCompCommand::getResult() {
        return std::string("deletion successful");
    }

    // --- Delelete Connection Command ---
    DelConnectionCommand::DelConnectionCommand(const UUID &src, int srcPin, PinType srcType, const UUID &dst, int dstPin, PinType dstType)
        : m_src(src), m_srcPin(srcPin), m_srcType(srcType), m_dst(dst), m_dstPin(dstPin), m_dstType(dstType) {}

    bool DelConnectionCommand::execute() {
        SimulationEngine::instance().deleteConnection(m_src, m_srcType, m_srcPin, m_dst, m_dstType, m_dstPin);
        return true;
    }

    void DelConnectionCommand::undo() {
        SimulationEngine::instance().connectComponent(m_src, m_srcPin, m_srcType, m_dst, m_dstPin, m_dstType);
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

    void SetInputCommand::undo() {
        SimulationEngine::instance().setDigitalInput(m_compId, m_oldState);
    }

    std::any SetInputCommand::getResult() {
        return m_newState;
    }
} // namespace Bess::SimEngine::Commands
