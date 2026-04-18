#pragma once

#include "bess_api.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Bess::SimEngine {
    using AnalogNodeId = uint32_t;

    inline constexpr AnalogNodeId AnalogGroundNode = 0;

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

    struct BESS_API AnalogSolution {
        AnalogSolveStatus status = AnalogSolveStatus::emptyCircuit;
        std::string message;
        std::vector<double> nodeVoltages;
        std::unordered_map<std::string, double> branchCurrents;

        bool ok() const;
        double voltage(AnalogNodeId node) const;
        std::optional<double> branchCurrent(const std::string &name) const;
    };

    class BESS_API AnalogStampContext {
      public:
        AnalogStampContext(std::vector<std::vector<double>> &matrix,
                           std::vector<double> &rhs,
                           std::unordered_map<std::string, size_t> &branchCurrentIndices,
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
        size_t m_nodeUnknownCount = 0;
        size_t m_nextVoltageSourceIndex = 0;
        bool m_ok = true;
        std::string m_errorMessage;
    };

    class BESS_API AnalogComponent {
      public:
        virtual ~AnalogComponent() = default;

        virtual bool validate(std::string &error, const AnalogSolveOptions &options) const;
        virtual void stamp(AnalogStampContext &context) const = 0;
        virtual size_t voltageSourceCount() const;
        virtual std::vector<AnalogNodeId> terminals() const = 0;
        virtual std::string name() const = 0;
    };

    class BESS_API Resistor final : public AnalogComponent {
      public:
        Resistor(AnalogNodeId a, AnalogNodeId b, double resistanceOhms, std::string name = {});

        bool validate(std::string &error, const AnalogSolveOptions &options) const override;
        void stamp(AnalogStampContext &context) const override;
        std::vector<AnalogNodeId> terminals() const override;
        std::string name() const override;

        double resistanceOhms() const;

      private:
        AnalogNodeId m_a;
        AnalogNodeId m_b;
        double m_resistanceOhms;
        std::string m_name;
    };

    class BESS_API DCVoltageSource final : public AnalogComponent {
      public:
        DCVoltageSource(AnalogNodeId positive, AnalogNodeId negative, double voltage, std::string name = {});

        bool validate(std::string &error, const AnalogSolveOptions &options) const override;
        void stamp(AnalogStampContext &context) const override;
        size_t voltageSourceCount() const override;
        std::vector<AnalogNodeId> terminals() const override;
        std::string name() const override;

      private:
        AnalogNodeId m_positive;
        AnalogNodeId m_negative;
        double m_voltage;
        std::string m_name;
    };

    class BESS_API DCCurrentSource final : public AnalogComponent {
      public:
        DCCurrentSource(AnalogNodeId positive, AnalogNodeId negative, double currentAmps, std::string name = {});

        bool validate(std::string &error, const AnalogSolveOptions &options) const override;
        void stamp(AnalogStampContext &context) const override;
        std::vector<AnalogNodeId> terminals() const override;
        std::string name() const override;

      private:
        AnalogNodeId m_positive;
        AnalogNodeId m_negative;
        double m_currentAmps;
        std::string m_name;
    };

    class BESS_API AnalogCircuit {
      public:
        AnalogCircuit();

        void clear();

        AnalogNodeId createNode(const std::string &name = {});
        void setNodeName(AnalogNodeId node, const std::string &name);
        std::string getNodeName(AnalogNodeId node) const;
        size_t nodeCount() const;

        void addComponent(std::shared_ptr<AnalogComponent> component);
        void addResistor(AnalogNodeId a, AnalogNodeId b, double resistanceOhms, const std::string &name = {});
        void addVoltageSource(AnalogNodeId positive, AnalogNodeId negative, double voltage,
                              const std::string &name = {});
        void addCurrentSource(AnalogNodeId positive, AnalogNodeId negative, double currentAmps,
                              const std::string &name = {});

        const std::vector<std::shared_ptr<AnalogComponent>> &components() const;
        AnalogSolution solve(const AnalogSolveOptions &options = {}) const;

      private:
        AnalogSolution validateCircuit(const AnalogSolveOptions &options) const;

        AnalogNodeId m_nextNodeId = 1;
        std::unordered_map<AnalogNodeId, std::string> m_nodeNames;
        std::vector<std::shared_ptr<AnalogComponent>> m_components;
    };
} // namespace Bess::SimEngine
