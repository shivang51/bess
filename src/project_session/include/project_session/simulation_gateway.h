#pragma once

#include "common/bess_api.h"
#include "common/bess_uuid.h"
#include "common/types.h"
#include "project_session/result.h"

#include <memory>

namespace Bess::SimEngine {
    class SimulationEngine;
}

namespace Bess::Session {
    struct BESS_API SimulationComponentInfo {
        UUID id = UUID::null;
        SimEngine::CompDefRef definition;
        SimEngine::PortDescriptor inputs;
        SimEngine::PortDescriptor outputs;
    };

    class BESS_API SimulationRestorePoint {
      public:
        virtual ~SimulationRestorePoint() = default;

        virtual UUID id() const noexcept = 0;
    };

    class BESS_API ISimulationGateway {
      public:
        virtual ~ISimulationGateway() = default;

        virtual Result<SimulationComponentInfo>
        createComponent(const SimEngine::CompDefRef &definition) = 0;
        virtual Result<std::unique_ptr<SimulationRestorePoint>>
        removeComponent(UUID id) = 0;
        virtual Result<SimulationComponentInfo>
        restoreComponent(const SimulationRestorePoint &restorePoint) = 0;
        virtual Result<SimulationComponentInfo>
        componentInfo(UUID id) const = 0;

        virtual Status connect(const SimEngine::PortRef &source,
                               const SimEngine::PortRef &destination) = 0;
        virtual Status disconnect(const SimEngine::PortRef &a,
                                  const SimEngine::PortRef &b) = 0;
        virtual Result<SimulationComponentInfo>
        addPort(const SimEngine::PortRef &port) = 0;
        virtual Result<SimulationComponentInfo>
        removePort(const SimEngine::PortRef &port) = 0;

        virtual Result<SimEngine::PortState>
        portState(const SimEngine::PortRef &port) const = 0;
        virtual SimEngine::SimulationState simulationState() const = 0;
        virtual Status setSimulationState(SimEngine::SimulationState state) = 0;
        virtual Status stepSimulation() = 0;
        virtual Status reset() = 0;
    };

    // Adapts the existing engine to the narrow interface used by the project
    // model. Tests and headless tools can provide their own gateway without
    // constructing application globals or simulation threads.
    class BESS_API SimulationEngineGateway final : public ISimulationGateway {
      public:
        explicit SimulationEngineGateway(
            SimEngine::SimulationEngine &simulationEngine);
        ~SimulationEngineGateway() override = default;

        Result<SimulationComponentInfo>
        createComponent(const SimEngine::CompDefRef &definition) override;
        Result<std::unique_ptr<SimulationRestorePoint>>
        removeComponent(UUID id) override;
        Result<SimulationComponentInfo>
        restoreComponent(const SimulationRestorePoint &restorePoint) override;
        Result<SimulationComponentInfo> componentInfo(UUID id) const override;

        Status connect(const SimEngine::PortRef &source,
                       const SimEngine::PortRef &destination) override;
        Status disconnect(const SimEngine::PortRef &a,
                          const SimEngine::PortRef &b) override;
        Result<SimulationComponentInfo>
        addPort(const SimEngine::PortRef &port) override;
        Result<SimulationComponentInfo>
        removePort(const SimEngine::PortRef &port) override;

        Result<SimEngine::PortState>
        portState(const SimEngine::PortRef &port) const override;
        SimEngine::SimulationState simulationState() const override;
        Status setSimulationState(SimEngine::SimulationState state) override;
        Status stepSimulation() override;
        Status reset() override;

      private:
        SimEngine::SimulationEngine *m_simulationEngine;
    };
} // namespace Bess::Session
