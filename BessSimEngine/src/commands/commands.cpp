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
} // namespace Bess::SimEngine::Commands
