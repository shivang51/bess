#include "project_session/simulation_gateway.h"

#include "component_catalog.h"
#include "sim_driver/sim_driver.h"
#include "simulation_engine.h"

#include <memory>
#include <string>

namespace Bess::Session {
    namespace {
        class NativeSimulationRestorePoint final
            : public SimulationRestorePoint {
          public:
            NativeSimulationRestorePoint(
                std::shared_ptr<SimEngine::Drivers::SimDriver> driver,
                std::shared_ptr<SimEngine::Drivers::SimComponent> component,
                SimEngine::CompDefRef definition)
                : m_driver(std::move(driver)),
                  m_component(std::move(component)),
                  m_definition(std::move(definition)) {
            }

            UUID id() const noexcept override {
                return m_component ? m_component->getUuid() : UUID::null;
            }

            std::shared_ptr<SimEngine::Drivers::SimDriver> driver() const {
                return m_driver;
            }

            std::shared_ptr<SimEngine::Drivers::SimComponent>
            component() const {
                return m_component;
            }

            const SimEngine::CompDefRef &definition() const {
                return m_definition;
            }

          private:
            std::shared_ptr<SimEngine::Drivers::SimDriver> m_driver;
            std::shared_ptr<SimEngine::Drivers::SimComponent> m_component;
            SimEngine::CompDefRef m_definition;
        };

        SimulationComponentInfo makeInfo(
            UUID id,
            SimEngine::CompDefRef definition,
            const std::shared_ptr<SimEngine::Drivers::CompDef> &nativeDef) {
            SimulationComponentInfo info;
            info.id = id;
            info.definition = std::move(definition);
            if (nativeDef) {
                info.inputs = nativeDef->getInputPortDescriptor();
                info.outputs = nativeDef->getOutputPortDescriptor();
            }
            return info;
        }
    } // namespace

    SimulationEngineGateway::SimulationEngineGateway(
        SimEngine::SimulationEngine &simulationEngine)
        : m_simulationEngine(&simulationEngine) {
    }

    Result<SimulationComponentInfo> SimulationEngineGateway::createComponent(
        const SimEngine::CompDefRef &definition) {
        if (definition.name.empty()) {
            return fail(Error::invalidArgument(
                "Simulation component definition name is empty"));
        }

        auto nativeDefinition =
            SimEngine::ComponentCatalog::instance().getComponentDefinition(
                definition.name);
        if (!nativeDefinition) {
            return fail(Error::notFound("Simulation component definition '" +
                                        definition.name + "' was not found"));
        }

        const auto id = m_simulationEngine->addComponent(nativeDefinition);
        if (id == UUID::null) {
            return fail(Error::simulationFailure(
                "The simulation engine rejected component definition '" +
                definition.name + "'"));
        }

        return makeInfo(id, definition, nativeDefinition);
    }

    Result<std::unique_ptr<SimulationRestorePoint>>
    SimulationEngineGateway::removeComponent(UUID id) {
        if (id == UUID::null) {
            return fail(Error::invalidArgument(
                "Cannot remove a null simulation component UUID"));
        }

        for (const auto &driver : m_simulationEngine->getDrivers()) {
            if (!driver || !driver->hasComponent(id)) {
                continue;
            }

            auto component =
                driver->getComponentSP<SimEngine::Drivers::SimComponent>(id);
            if (!component || !component->getDefinition()) {
                return fail(Error::simulationFailure(
                    "Simulation component has no restorable definition"));
            }

            SimEngine::CompDefRef reference;
            reference.name = component->getDefinition()->getName();
            auto restorePoint = std::make_unique<NativeSimulationRestorePoint>(
                driver, component, std::move(reference));
            m_simulationEngine->deleteComponent(id);
            if (driver->hasComponent(id)) {
                return fail(Error::simulationFailure(
                    "Simulation engine did not remove the component"));
            }
            return std::unique_ptr<SimulationRestorePoint>(
                std::move(restorePoint));
        }

        return fail(Error::notFound("Simulation component does not exist"));
    }

    Result<SimulationComponentInfo> SimulationEngineGateway::restoreComponent(
        const SimulationRestorePoint &restorePoint) {
        const auto *native =
            dynamic_cast<const NativeSimulationRestorePoint *>(&restorePoint);
        if (!native || !native->driver() || !native->component()) {
            return fail(Error::invalidArgument(
                "Restore point was not created by this simulation gateway"));
        }
        if (native->driver()->hasComponent(native->id())) {
            return fail(Error::alreadyExists(
                "Simulation component is already present"));
        }

        const auto restoredId =
            native->driver()->addComponent(native->component(), true);
        if (restoredId != native->id()) {
            return fail(Error::simulationFailure(
                "Simulation driver failed to preserve the component UUID"));
        }
        return makeInfo(restoredId,
                        native->definition(),
                        native->component()->getDefinition());
    }

    Result<SimulationComponentInfo>
    SimulationEngineGateway::componentInfo(UUID id) const {
        if (id == UUID::null) {
            return fail(
                Error::invalidArgument("Simulation component UUID is null"));
        }
        const auto &definition = m_simulationEngine->getComponentDefinition(id);
        if (!definition) {
            return fail(Error::notFound("Simulation component does not exist"));
        }
        SimEngine::CompDefRef reference;
        reference.name = definition->getName();
        return makeInfo(id, std::move(reference), definition);
    }

    Status
    SimulationEngineGateway::connect(const SimEngine::PortRef &source,
                                     const SimEngine::PortRef &destination) {
        const auto [canConnect, message] =
            m_simulationEngine->canConnectPorts(source, destination);
        if (!canConnect) {
            return fail(Error::conflict(
                message.empty() ? "Simulation ports cannot be connected"
                                : message));
        }
        if (!m_simulationEngine->connectPorts(source, destination)) {
            return fail(Error::simulationFailure(
                "Simulation engine failed to connect the ports"));
        }
        return {};
    }

    Status SimulationEngineGateway::disconnect(const SimEngine::PortRef &a,
                                               const SimEngine::PortRef &b) {
        if (!a.isValid() || !b.isValid()) {
            return fail(Error::invalidArgument(
                "Cannot disconnect invalid simulation ports"));
        }
        m_simulationEngine->deleteConnection(a, b);
        return {};
    }

    Result<SimulationComponentInfo>
    SimulationEngineGateway::addPort(const SimEngine::PortRef &port) {
        if (!port.isValid()) {
            return fail(Error::invalidArgument("Cannot add an invalid port"));
        }
        if (!m_simulationEngine->addPort(port)) {
            return fail(Error::simulationFailure(
                "Simulation engine failed to add the port"));
        }
        return componentInfo(port.componentId);
    }

    Result<SimulationComponentInfo>
    SimulationEngineGateway::removePort(const SimEngine::PortRef &port) {
        if (!port.isValid()) {
            return fail(
                Error::invalidArgument("Cannot remove an invalid port"));
        }
        if (!m_simulationEngine->removePort(port)) {
            return fail(Error::simulationFailure(
                "Simulation engine failed to remove the port"));
        }
        return componentInfo(port.componentId);
    }

    Result<SimEngine::PortState>
    SimulationEngineGateway::portState(const SimEngine::PortRef &port) const {
        if (!port.isValid()) {
            return fail(Error::invalidArgument("Cannot query an invalid port"));
        }
        return m_simulationEngine->getPortState(port);
    }

    SimEngine::SimulationState
    SimulationEngineGateway::simulationState() const {
        return m_simulationEngine->getSimulationState();
    }

    Status SimulationEngineGateway::setSimulationState(
        SimEngine::SimulationState state) {
        m_simulationEngine->setSimulationState(state);
        return {};
    }

    Status SimulationEngineGateway::stepSimulation() {
        if (m_simulationEngine->getSimulationState() !=
            SimEngine::SimulationState::paused) {
            return fail(
                Error::conflict("Simulation can only step while paused"));
        }
        m_simulationEngine->stepSimulation();
        return {};
    }

    Status SimulationEngineGateway::reset() {
        m_simulationEngine->clear(false);
        return {};
    }
} // namespace Bess::Session
