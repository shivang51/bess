#pragma once

#include "bess_api.h"
#include "common/bess_uuid.h"
#include "component_definition.h"
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Bess::SimEngine {
    using AnalogNodeId = uint32_t;

    inline constexpr AnalogNodeId AnalogGroundNode = 0;
    inline constexpr AnalogNodeId AnalogUnconnectedNode = std::numeric_limits<AnalogNodeId>::max();

    enum class AnalogSolveStatus : uint8_t {
        ok,
        emptyCircuit,
        invalidComponent,
        singularMatrix,
        nonFiniteValue,
        internalError
    };

    struct BESS_API AnalogSolveOptions {
        double pivotTolerance = 1e-12;
        double minResistanceOhms = 1e-12;
    };

    struct BESS_API AnalogTerminalState {
        AnalogNodeId node = AnalogUnconnectedNode;
        double voltage = 0.0;
        double current = 0.0;
        bool connected = false;
    };

    struct BESS_API AnalogComponentState {
        UUID id = UUID::null;
        std::string name;
        std::vector<AnalogTerminalState> terminals;
        bool simError = false;
        std::string errorMessage;
    };

    struct BESS_API AnalogSolution {
        AnalogSolveStatus status = AnalogSolveStatus::emptyCircuit;
        std::string message;
        std::vector<double> nodeVoltages;
        std::unordered_map<std::string, double> branchCurrents;
        std::unordered_map<UUID, AnalogComponentState> componentStates;

        bool ok() const;
        double voltage(AnalogNodeId node) const;
        std::optional<double> branchCurrent(const std::string &name) const;
        const AnalogComponentState *componentState(const UUID &id) const;
    };

    class BESS_API AnalogStampContext {
      public:
        AnalogStampContext(std::vector<std::vector<double>> &matrix,
                           std::vector<double> &rhs,
                           std::unordered_map<std::string, size_t> &branchCurrentIndices,
                           const std::unordered_map<AnalogNodeId, size_t> &nodeIndexLookup,
                           size_t nodeUnknownCount);

        void addConductance(AnalogNodeId a, AnalogNodeId b, double conductanceSiemens);
        void addCurrentSource(AnalogNodeId positive, AnalogNodeId negative, double currentAmps);
        void addVoltageSource(AnalogNodeId positive, AnalogNodeId negative, double voltage,
                              const std::string &branchName = {});

        bool ok() const;
        const std::string &errorMessage() const;

      private:
        std::optional<size_t> matrixIndex(AnalogNodeId node) const;
        void invalidate(const std::string &message);

        std::vector<std::vector<double>> &m_matrix;
        std::vector<double> &m_rhs;
        std::unordered_map<std::string, size_t> &m_branchCurrentIndices;
        const std::unordered_map<AnalogNodeId, size_t> &m_nodeIndexLookup;
        size_t m_nodeUnknownCount = 0;
        size_t m_nextVoltageSourceIndex = 0;
        bool m_ok = true;
        std::string m_errorMessage;
    };

    class BESS_API AnalogComponent {
      public:
        virtual ~AnalogComponent() = default;

        const UUID &getUUID() const;
        void setUUID(const UUID &id);

        virtual bool validate(std::string &error, const AnalogSolveOptions &options) const;
        virtual void stamp(AnalogStampContext &context) const = 0;
        virtual AnalogComponentState evaluateState(const AnalogSolution &solution) const;
        virtual size_t voltageSourceCount() const;
        virtual std::vector<AnalogNodeId> terminals() const = 0;
        virtual bool setTerminalNode(size_t terminalIdx, AnalogNodeId node);
        virtual std::string name() const = 0;

        // Generic metadata hooks for plugin-defined components.
        virtual std::optional<double> numericValue() const;
        virtual bool setNumericValue(double value);
        virtual std::optional<std::string> branchCurrentName() const;

      private:
        UUID m_id;
    };

    class BESS_API AnalogComponentTrait : public Trait {
      public:
        using Factory = std::function<std::shared_ptr<AnalogComponent>()>;

        AnalogComponentTrait() = default;
        AnalogComponentTrait(size_t terminalCount,
                             std::vector<std::string> terminalNames,
                             Factory factory);

        std::shared_ptr<Trait> clone() const override;
        Json::Value toJson() const override;

        size_t terminalCount = 0;
        std::vector<std::string> terminalNames;
        Factory factory;
    };

    class BESS_API AnalogCircuit {
      public:
        AnalogCircuit();

        void clear();

        AnalogNodeId createNode(const std::string &name = {});
        void setNodeName(AnalogNodeId node, const std::string &name);
        std::string getNodeName(AnalogNodeId node) const;
        size_t nodeCount() const;

        const UUID &addComponent(std::shared_ptr<AnalogComponent> component);
        const UUID &addResistor(double resistanceOhms, const std::string &name = {});
        const UUID &addResistor(AnalogNodeId a, AnalogNodeId b, double resistanceOhms, const std::string &name = {});
        const UUID &addVoltageSource(double voltage, const std::string &name = {});
        const UUID &addVoltageSource(AnalogNodeId positive, AnalogNodeId negative, double voltage,
                                     const std::string &name = {});
        const UUID &addCurrentSource(double currentAmps, const std::string &name = {});
        const UUID &addCurrentSource(AnalogNodeId positive, AnalogNodeId negative, double currentAmps,
                                     const std::string &name = {});
        const UUID &addTestPoint(const std::string &name = {});
        const UUID &addTestPoint(AnalogNodeId node, const std::string &name = {});
        const UUID &addVoltageProbe(const std::string &name = {});
        const UUID &addVoltageProbe(AnalogNodeId positive, AnalogNodeId negative, const std::string &name = {});
        const UUID &addCurrentProbe(const std::string &name = {});
        const UUID &addCurrentProbe(AnalogNodeId positive, AnalogNodeId negative, const std::string &name = {});
        const UUID &addGroundReference(const std::string &name = {});
        const UUID &addGroundReference(AnalogNodeId node, const std::string &name = {});

        bool connectTerminal(const UUID &componentId, size_t terminalIdx, AnalogNodeId node);
        bool disconnectTerminal(const UUID &componentId, size_t terminalIdx);
        bool removeComponent(const UUID &componentId);

        std::shared_ptr<AnalogComponent> getComponent(const UUID &componentId) const;
        AnalogComponentState getComponentState(const UUID &componentId) const;
        std::optional<double> getResistorResistance(const UUID &componentId) const;
        bool setResistorResistance(const UUID &componentId, double resistanceOhms);
        std::optional<double> getVoltageSourceVoltage(const UUID &componentId) const;
        bool setVoltageSourceVoltage(const UUID &componentId, double voltage);

        const std::vector<std::shared_ptr<AnalogComponent>> &components() const;
        const AnalogSolution &lastSolution() const;
        AnalogSolution solve(const AnalogSolveOptions &options = {}) const;

      private:
        AnalogSolution validateCircuit(const AnalogSolveOptions &options) const;
        AnalogComponentState makeInvalidState(const AnalogComponent &component, const std::string &message) const;

        AnalogNodeId m_nextNodeId = 1;
        std::unordered_map<AnalogNodeId, std::string> m_nodeNames;
        std::vector<std::shared_ptr<AnalogComponent>> m_components;
        std::unordered_map<UUID, size_t> m_componentIndices;
        mutable AnalogSolution m_lastSolution;
    };
} // namespace Bess::SimEngine
