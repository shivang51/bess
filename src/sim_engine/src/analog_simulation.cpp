#include "analog_simulation.h"
#include <algorithm>
#include <cstddef>
#include <cmath>
#include <limits>
#include <utility>

namespace Bess::SimEngine {
    namespace {
        bool isFinite(double value) {
            return std::isfinite(value);
        }

        AnalogSolution failure(AnalogSolveStatus status, std::string message) {
            AnalogSolution solution;
            solution.status = status;
            solution.message = std::move(message);
            return solution;
        }

        bool solveLinearSystem(std::vector<std::vector<double>> matrix,
                               std::vector<double> rhs,
                               double pivotTolerance,
                               std::vector<double> &solution) {
            const size_t n = rhs.size();
            solution.assign(n, 0.0);

            for (size_t column = 0; column < n; ++column) {
                size_t pivot = column;
                double best = std::abs(matrix[column][column]);
                for (size_t row = column + 1; row < n; ++row) {
                    const double candidate = std::abs(matrix[row][column]);
                    if (candidate > best) {
                        best = candidate;
                        pivot = row;
                    }
                }

                if (best <= pivotTolerance) {
                    return false;
                }

                if (pivot != column) {
                    std::swap(matrix[pivot], matrix[column]);
                    std::swap(rhs[pivot], rhs[column]);
                }

                const double pivotValue = matrix[column][column];
                for (size_t row = column + 1; row < n; ++row) {
                    const double factor = matrix[row][column] / pivotValue;
                    if (factor == 0.0) {
                        continue;
                    }
                    matrix[row][column] = 0.0;
                    for (size_t col = column + 1; col < n; ++col) {
                        matrix[row][col] -= factor * matrix[column][col];
                    }
                    rhs[row] -= factor * rhs[column];
                }
            }

            for (size_t row = n; row-- > 0;) {
                double sum = rhs[row];
                for (size_t col = row + 1; col < n; ++col) {
                    sum -= matrix[row][col] * solution[col];
                }
                solution[row] = sum / matrix[row][row];
                if (!isFinite(solution[row])) {
                    return false;
                }
            }

            return true;
        }
    } // namespace

    bool AnalogSolution::ok() const {
        return status == AnalogSolveStatus::ok;
    }

    double AnalogSolution::voltage(AnalogNodeId node) const {
        if (node >= nodeVoltages.size()) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        return nodeVoltages[node];
    }

    std::optional<double> AnalogSolution::branchCurrent(const std::string &name) const {
        const auto it = branchCurrents.find(name);
        if (it == branchCurrents.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    const AnalogComponentState *AnalogSolution::componentState(const UUID &id) const {
        const auto it = componentStates.find(id);
        if (it == componentStates.end()) {
            return nullptr;
        }
        return &it->second;
    }

    AnalogStampContext::AnalogStampContext(std::vector<std::vector<double>> &matrix,
                                           std::vector<double> &rhs,
                                           std::unordered_map<std::string, size_t> &branchCurrentIndices,
                                           size_t nodeUnknownCount)
        : m_matrix(matrix),
          m_rhs(rhs),
          m_branchCurrentIndices(branchCurrentIndices),
          m_nodeUnknownCount(nodeUnknownCount) {}

    std::optional<size_t> AnalogStampContext::matrixIndex(AnalogNodeId node) const {
        if (node == AnalogGroundNode) {
            return std::nullopt;
        }
        const size_t index = static_cast<size_t>(node - 1);
        if (index >= m_nodeUnknownCount) {
            return std::nullopt;
        }
        return index;
    }

    void AnalogStampContext::invalidate(const std::string &message) {
        if (m_ok) {
            m_errorMessage = message;
        }
        m_ok = false;
    }

    void AnalogStampContext::addConductance(AnalogNodeId a, AnalogNodeId b, double conductanceSiemens) {
        if (!m_ok) {
            return;
        }
        if (!isFinite(conductanceSiemens)) {
            invalidate("Attempted to stamp a non-finite conductance");
            return;
        }

        const auto ia = matrixIndex(a);
        const auto ib = matrixIndex(b);

        if (ia) {
            m_matrix[*ia][*ia] += conductanceSiemens;
        }
        if (ib) {
            m_matrix[*ib][*ib] += conductanceSiemens;
        }
        if (ia && ib) {
            m_matrix[*ia][*ib] -= conductanceSiemens;
            m_matrix[*ib][*ia] -= conductanceSiemens;
        }
    }

    void AnalogStampContext::addCurrentSource(AnalogNodeId positive, AnalogNodeId negative, double currentAmps) {
        if (!m_ok) {
            return;
        }
        if (!isFinite(currentAmps)) {
            invalidate("Attempted to stamp a non-finite current source");
            return;
        }

        const auto ip = matrixIndex(positive);
        const auto in = matrixIndex(negative);

        if (ip) {
            m_rhs[*ip] -= currentAmps;
        }
        if (in) {
            m_rhs[*in] += currentAmps;
        }
    }

    void AnalogStampContext::addVoltageSource(AnalogNodeId positive, AnalogNodeId negative, double voltage,
                                              const std::string &branchName) {
        if (!m_ok) {
            return;
        }
        if (!isFinite(voltage)) {
            invalidate("Attempted to stamp a non-finite voltage source");
            return;
        }

        const size_t branchIndex = m_nodeUnknownCount + m_nextVoltageSourceIndex++;
        if (branchIndex >= m_matrix.size()) {
            invalidate("A component stamped more voltage sources than it declared");
            return;
        }

        const auto ip = matrixIndex(positive);
        const auto in = matrixIndex(negative);

        if (ip) {
            m_matrix[*ip][branchIndex] += 1.0;
            m_matrix[branchIndex][*ip] += 1.0;
        }
        if (in) {
            m_matrix[*in][branchIndex] -= 1.0;
            m_matrix[branchIndex][*in] -= 1.0;
        }

        m_rhs[branchIndex] += voltage;
        if (!branchName.empty()) {
            m_branchCurrentIndices[branchName] = branchIndex;
        }
    }

    bool AnalogStampContext::ok() const {
        return m_ok;
    }

    const std::string &AnalogStampContext::errorMessage() const {
        return m_errorMessage;
    }

    const UUID &AnalogComponent::getUUID() const {
        return m_id;
    }

    void AnalogComponent::setUUID(const UUID &id) {
        m_id = id;
    }

    bool AnalogComponent::validate(std::string &, const AnalogSolveOptions &) const {
        return true;
    }

    AnalogComponentState AnalogComponent::evaluateState(const AnalogSolution &solution) const {
        AnalogComponentState state;
        state.id = getUUID();
        state.name = name();
        for (const auto node : terminals()) {
            AnalogTerminalState terminalState;
            terminalState.node = node;
            terminalState.connected = node != AnalogUnconnectedNode;
            terminalState.voltage = terminalState.connected ? solution.voltage(node) : 0.0;
            state.terminals.push_back(terminalState);
        }
        return state;
    }

    size_t AnalogComponent::voltageSourceCount() const {
        return 0;
    }

    bool AnalogComponent::setTerminalNode(size_t, AnalogNodeId) {
        return false;
    }

    Resistor::Resistor(double resistanceOhms, std::string name)
        : m_terminals({AnalogUnconnectedNode, AnalogUnconnectedNode}),
          m_resistanceOhms(resistanceOhms),
          m_name(std::move(name)) {}

    Resistor::Resistor(AnalogNodeId a, AnalogNodeId b, double resistanceOhms, std::string name)
        : m_terminals({a, b}),
          m_resistanceOhms(resistanceOhms),
          m_name(std::move(name)) {}

    bool Resistor::validate(std::string &error, const AnalogSolveOptions &options) const {
        if (m_terminals[0] == AnalogUnconnectedNode || m_terminals[1] == AnalogUnconnectedNode) {
            error = "Resistor terminals must both be connected";
            return false;
        }
        if (m_terminals[0] == m_terminals[1]) {
            error = "Resistor terminals must be connected to different nodes";
            return false;
        }
        if (!isFinite(m_resistanceOhms) || m_resistanceOhms < options.minResistanceOhms) {
            error = "Resistor resistance must be finite and greater than the configured minimum";
            return false;
        }
        return true;
    }

    void Resistor::stamp(AnalogStampContext &context) const {
        context.addConductance(m_terminals[0], m_terminals[1], 1.0 / m_resistanceOhms);
    }

    AnalogComponentState Resistor::evaluateState(const AnalogSolution &solution) const {
        auto state = AnalogComponent::evaluateState(solution);
        if (state.terminals.size() == 2 && state.terminals[0].connected && state.terminals[1].connected) {
            const double current = (state.terminals[0].voltage - state.terminals[1].voltage) / m_resistanceOhms;
            state.terminals[0].current = current;
            state.terminals[1].current = -current;
        }
        return state;
    }

    std::vector<AnalogNodeId> Resistor::terminals() const {
        return m_terminals;
    }

    bool Resistor::setTerminalNode(size_t terminalIdx, AnalogNodeId node) {
        if (terminalIdx >= m_terminals.size()) {
            return false;
        }
        m_terminals[terminalIdx] = node;
        return true;
    }

    std::string Resistor::name() const {
        return m_name.empty() ? "Resistor" : m_name;
    }

    double Resistor::resistanceOhms() const {
        return m_resistanceOhms;
    }

    DCVoltageSource::DCVoltageSource(double voltage, std::string name)
        : m_terminals({AnalogUnconnectedNode, AnalogUnconnectedNode}),
          m_voltage(voltage),
          m_name(std::move(name)) {}

    DCVoltageSource::DCVoltageSource(AnalogNodeId positive, AnalogNodeId negative, double voltage, std::string name)
        : m_terminals({positive, negative}),
          m_voltage(voltage),
          m_name(std::move(name)) {}

    bool DCVoltageSource::validate(std::string &error, const AnalogSolveOptions &) const {
        if (m_terminals[0] == AnalogUnconnectedNode || m_terminals[1] == AnalogUnconnectedNode) {
            error = "Voltage source terminals must both be connected";
            return false;
        }
        if (m_terminals[0] == m_terminals[1]) {
            error = "Voltage source terminals must be connected to different nodes";
            return false;
        }
        if (!isFinite(m_voltage)) {
            error = "Voltage source value must be finite";
            return false;
        }
        return true;
    }

    void DCVoltageSource::stamp(AnalogStampContext &context) const {
        context.addVoltageSource(m_terminals[0], m_terminals[1], m_voltage,
                                 m_name.empty() ? getUUID().toString() : m_name);
    }

    AnalogComponentState DCVoltageSource::evaluateState(const AnalogSolution &solution) const {
        auto state = AnalogComponent::evaluateState(solution);
        const auto current = solution.branchCurrent(m_name.empty() ? getUUID().toString() : m_name);
        if (current && state.terminals.size() == 2) {
            state.terminals[0].current = *current;
            state.terminals[1].current = -*current;
        }
        return state;
    }

    size_t DCVoltageSource::voltageSourceCount() const {
        return 1;
    }

    std::vector<AnalogNodeId> DCVoltageSource::terminals() const {
        return m_terminals;
    }

    bool DCVoltageSource::setTerminalNode(size_t terminalIdx, AnalogNodeId node) {
        if (terminalIdx >= m_terminals.size()) {
            return false;
        }
        m_terminals[terminalIdx] = node;
        return true;
    }

    std::string DCVoltageSource::name() const {
        return m_name.empty() ? "DCVoltageSource" : m_name;
    }

    DCCurrentSource::DCCurrentSource(double currentAmps, std::string name)
        : m_terminals({AnalogUnconnectedNode, AnalogUnconnectedNode}),
          m_currentAmps(currentAmps),
          m_name(std::move(name)) {}

    DCCurrentSource::DCCurrentSource(AnalogNodeId positive, AnalogNodeId negative, double currentAmps, std::string name)
        : m_terminals({positive, negative}),
          m_currentAmps(currentAmps),
          m_name(std::move(name)) {}

    bool DCCurrentSource::validate(std::string &error, const AnalogSolveOptions &) const {
        if (m_terminals[0] == AnalogUnconnectedNode || m_terminals[1] == AnalogUnconnectedNode) {
            error = "Current source terminals must both be connected";
            return false;
        }
        if (m_terminals[0] == m_terminals[1]) {
            error = "Current source terminals must be connected to different nodes";
            return false;
        }
        if (!isFinite(m_currentAmps)) {
            error = "Current source value must be finite";
            return false;
        }
        return true;
    }

    void DCCurrentSource::stamp(AnalogStampContext &context) const {
        context.addCurrentSource(m_terminals[0], m_terminals[1], m_currentAmps);
    }

    AnalogComponentState DCCurrentSource::evaluateState(const AnalogSolution &solution) const {
        auto state = AnalogComponent::evaluateState(solution);
        if (state.terminals.size() == 2) {
            state.terminals[0].current = m_currentAmps;
            state.terminals[1].current = -m_currentAmps;
        }
        return state;
    }

    std::vector<AnalogNodeId> DCCurrentSource::terminals() const {
        return m_terminals;
    }

    bool DCCurrentSource::setTerminalNode(size_t terminalIdx, AnalogNodeId node) {
        if (terminalIdx >= m_terminals.size()) {
            return false;
        }
        m_terminals[terminalIdx] = node;
        return true;
    }

    std::string DCCurrentSource::name() const {
        return m_name.empty() ? "DCCurrentSource" : m_name;
    }

    AnalogCircuit::AnalogCircuit() {
        m_nodeNames[AnalogGroundNode] = "0";
    }

    void AnalogCircuit::clear() {
        m_nextNodeId = 1;
        m_nodeNames.clear();
        m_nodeNames[AnalogGroundNode] = "0";
        m_components.clear();
        m_componentIndices.clear();
        m_lastSolution = {};
    }

    AnalogNodeId AnalogCircuit::createNode(const std::string &name) {
        const AnalogNodeId node = m_nextNodeId++;
        if (!name.empty()) {
            m_nodeNames[node] = name;
        }
        m_lastSolution = {};
        return node;
    }

    void AnalogCircuit::setNodeName(AnalogNodeId node, const std::string &name) {
        if (node < m_nextNodeId) {
            m_nodeNames[node] = name;
        }
    }

    std::string AnalogCircuit::getNodeName(AnalogNodeId node) const {
        const auto it = m_nodeNames.find(node);
        if (it == m_nodeNames.end()) {
            return {};
        }
        return it->second;
    }

    size_t AnalogCircuit::nodeCount() const {
        return m_nextNodeId;
    }

    const UUID &AnalogCircuit::addComponent(std::shared_ptr<AnalogComponent> component) {
        static const UUID nullComponentId = UUID::null;
        if (!component) {
            return nullComponentId;
        }

        if (component->getUUID() == UUID::null) {
            component->setUUID(UUID());
        }

        const auto &id = component->getUUID();
        if (m_componentIndices.contains(id)) {
            return id;
        }

        m_componentIndices[id] = m_components.size();
        m_components.push_back(std::move(component));
        m_lastSolution = {};
        return m_components.back()->getUUID();
    }

    const UUID &AnalogCircuit::addResistor(double resistanceOhms, const std::string &name) {
        return addComponent(std::make_shared<Resistor>(resistanceOhms, name));
    }

    const UUID &AnalogCircuit::addResistor(AnalogNodeId a, AnalogNodeId b, double resistanceOhms, const std::string &name) {
        return addComponent(std::make_shared<Resistor>(a, b, resistanceOhms, name));
    }

    const UUID &AnalogCircuit::addVoltageSource(double voltage, const std::string &name) {
        return addComponent(std::make_shared<DCVoltageSource>(voltage, name));
    }

    const UUID &AnalogCircuit::addVoltageSource(AnalogNodeId positive, AnalogNodeId negative, double voltage,
                                                const std::string &name) {
        return addComponent(std::make_shared<DCVoltageSource>(positive, negative, voltage, name));
    }

    const UUID &AnalogCircuit::addCurrentSource(double currentAmps, const std::string &name) {
        return addComponent(std::make_shared<DCCurrentSource>(currentAmps, name));
    }

    const UUID &AnalogCircuit::addCurrentSource(AnalogNodeId positive, AnalogNodeId negative, double currentAmps,
                                                const std::string &name) {
        return addComponent(std::make_shared<DCCurrentSource>(positive, negative, currentAmps, name));
    }

    bool AnalogCircuit::connectTerminal(const UUID &componentId, size_t terminalIdx, AnalogNodeId node) {
        if (node != AnalogGroundNode && node >= m_nextNodeId) {
            return false;
        }

        const auto component = getComponent(componentId);
        if (!component || !component->setTerminalNode(terminalIdx, node)) {
            return false;
        }

        m_lastSolution = {};
        return true;
    }

    bool AnalogCircuit::disconnectTerminal(const UUID &componentId, size_t terminalIdx) {
        const auto component = getComponent(componentId);
        if (!component || !component->setTerminalNode(terminalIdx, AnalogUnconnectedNode)) {
            return false;
        }

        m_lastSolution = {};
        return true;
    }

    bool AnalogCircuit::removeComponent(const UUID &componentId) {
        const auto it = m_componentIndices.find(componentId);
        if (it == m_componentIndices.end()) {
            return false;
        }

        m_components.erase(m_components.begin() + static_cast<std::ptrdiff_t>(it->second));
        m_componentIndices.clear();
        for (size_t i = 0; i < m_components.size(); ++i) {
            m_componentIndices[m_components[i]->getUUID()] = i;
        }

        m_lastSolution = {};
        return true;
    }

    std::shared_ptr<AnalogComponent> AnalogCircuit::getComponent(const UUID &componentId) const {
        const auto it = m_componentIndices.find(componentId);
        if (it == m_componentIndices.end() || it->second >= m_components.size()) {
            return nullptr;
        }
        return m_components[it->second];
    }

    AnalogComponentState AnalogCircuit::getComponentState(const UUID &componentId) const {
        if (const auto *state = m_lastSolution.componentState(componentId)) {
            return *state;
        }

        const auto component = getComponent(componentId);
        if (!component) {
            AnalogComponentState state;
            state.id = componentId;
            state.simError = true;
            state.errorMessage = "Analog component does not exist";
            return state;
        }

        return component->evaluateState(m_lastSolution);
    }

    const std::vector<std::shared_ptr<AnalogComponent>> &AnalogCircuit::components() const {
        return m_components;
    }

    const AnalogSolution &AnalogCircuit::lastSolution() const {
        return m_lastSolution;
    }

    AnalogSolution AnalogCircuit::validateCircuit(const AnalogSolveOptions &options) const {
        if (m_components.empty()) {
            return failure(AnalogSolveStatus::emptyCircuit, "Analog circuit has no components");
        }

        for (const auto &component : m_components) {
            if (!component) {
                return failure(AnalogSolveStatus::invalidComponent, "Analog circuit contains a null component");
            }

            std::string error;
            if (!component->validate(error, options)) {
                return failure(AnalogSolveStatus::invalidComponent, component->name() + ": " + error);
            }

            for (const auto node : component->terminals()) {
                if (node >= m_nextNodeId) {
                    return failure(AnalogSolveStatus::invalidComponent,
                                   component->name() + ": terminal references an unknown node");
                }
            }
        }

        AnalogSolution solution;
        solution.status = AnalogSolveStatus::ok;
        return solution;
    }

    AnalogComponentState AnalogCircuit::makeInvalidState(const AnalogComponent &component,
                                                         const std::string &message) const {
        AnalogSolution emptySolution;
        emptySolution.nodeVoltages.assign(m_nextNodeId, 0.0);
        auto state = component.evaluateState(emptySolution);
        state.simError = true;
        state.errorMessage = message;
        return state;
    }

    AnalogSolution AnalogCircuit::solve(const AnalogSolveOptions &options) const {
        const auto validation = validateCircuit(options);
        if (!validation.ok()) {
            m_lastSolution = validation;
            m_lastSolution.nodeVoltages.assign(m_nextNodeId, 0.0);
            for (const auto &component : m_components) {
                if (component) {
                    m_lastSolution.componentStates[component->getUUID()] =
                        makeInvalidState(*component, validation.message);
                }
            }
            return m_lastSolution;
        }

        const size_t nodeUnknownCount = static_cast<size_t>(m_nextNodeId - 1);
        size_t voltageSourceCount = 0;
        for (const auto &component : m_components) {
            voltageSourceCount += component->voltageSourceCount();
        }

        const size_t matrixSize = nodeUnknownCount + voltageSourceCount;
        if (matrixSize == 0) {
            AnalogSolution solution;
            solution.status = AnalogSolveStatus::ok;
            solution.message = "Only ground-referenced components were present";
            solution.nodeVoltages.assign(m_nextNodeId, 0.0);
            for (const auto &component : m_components) {
                solution.componentStates[component->getUUID()] = component->evaluateState(solution);
            }
            m_lastSolution = solution;
            return m_lastSolution;
        }

        std::vector<std::vector<double>> matrix(matrixSize, std::vector<double>(matrixSize, 0.0));
        std::vector<double> rhs(matrixSize, 0.0);
        std::unordered_map<std::string, size_t> branchCurrentIndices;
        AnalogStampContext context(matrix, rhs, branchCurrentIndices, nodeUnknownCount);

        for (const auto &component : m_components) {
            component->stamp(context);
            if (!context.ok()) {
                m_lastSolution = failure(AnalogSolveStatus::invalidComponent,
                                         component->name() + ": " + context.errorMessage());
                m_lastSolution.nodeVoltages.assign(m_nextNodeId, 0.0);
                for (const auto &stateComponent : m_components) {
                    m_lastSolution.componentStates[stateComponent->getUUID()] =
                        makeInvalidState(*stateComponent, m_lastSolution.message);
                }
                return m_lastSolution;
            }
        }

        std::vector<double> rawSolution;
        if (!solveLinearSystem(matrix, rhs, options.pivotTolerance, rawSolution)) {
            m_lastSolution = failure(AnalogSolveStatus::singularMatrix,
                                     "Analog solve failed because the MNA matrix is singular or ill-conditioned");
            m_lastSolution.nodeVoltages.assign(m_nextNodeId, 0.0);
            for (const auto &component : m_components) {
                m_lastSolution.componentStates[component->getUUID()] =
                    makeInvalidState(*component, m_lastSolution.message);
            }
            return m_lastSolution;
        }

        AnalogSolution solution;
        solution.status = AnalogSolveStatus::ok;
        solution.message = "Analog solve converged";
        solution.nodeVoltages.assign(m_nextNodeId, 0.0);
        for (AnalogNodeId node = 1; node < m_nextNodeId; ++node) {
            solution.nodeVoltages[node] = rawSolution[node - 1];
        }

        for (const auto &[name, index] : branchCurrentIndices) {
            if (index < rawSolution.size()) {
                solution.branchCurrents[name] = rawSolution[index];
            }
        }

        for (const auto &component : m_components) {
            solution.componentStates[component->getUUID()] = component->evaluateState(solution);
        }

        m_lastSolution = solution;
        return m_lastSolution;
    }
} // namespace Bess::SimEngine
