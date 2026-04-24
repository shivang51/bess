#include "drivers/digital_sim_driver.h"

namespace Bess::SimEngine::Drivers::Digital {
    std::string DigitalSimDriver::getName() const {
        return "Digital Simulation Driver";
    }

    bool DigitalSimDriver::simulate(const SimEvt &evt) {
        const auto &id = evt.compId;

        const auto &comp = this->template getComponent<DigitalSimComponent>(id);

        if (!comp) {
            BESS_WARN("(DigitalSimDriver.simulate) Component with UUID {} not found",
                      (uint64_t)id);
            return false;
        }

        auto simData = std::make_shared<DigCompSimData>();
        simData->simTime = m_currentSimTime;
        simData->prevState.inputStates = comp->getInputStates();
        simData->prevState.outputStates = comp->getOutputStates();
        simData->inputStates = collapseInputs(id);
        simData->outputStates = comp->getOutputStates();

        auto newData = std::dynamic_pointer_cast<DigCompSimData>(
            comp->simulate(simData));

        if (!newData) {
            BESS_WARN("(DigitalSimDriver.simulate) Simulation function for component with UUID {} did not return DigCompSimData",
                      (uint64_t)id);
            return false;
        }

        comp->setOutputStates(newData->outputStates);

        return newData->simDependants;
    }

    void DigitalSimDriver::addComponent(const std::shared_ptr<DigitalSimComponent> &comp) {
        EvtBasedSimDriver::addComponent(comp);
    }

    void DigitalSimDriver::onBeforeRun() {
        EvtBasedSimDriver::onBeforeRun();
        BESS_DEBUG("Starting DigitalSimDriver run loop");
    }

    std::shared_ptr<SimComponent> DigitalSimDriver::createComp(const std::shared_ptr<ComponentDef> &def) {
        if (!suuportsDef(def)) {
            BESS_WARN("(DigitalSimDriver.addComponent) Unsupported component definition type: {}",
                      def->getName());
            return nullptr;
        }

        const auto comp = DigitalSimComponent::fromDef(def->clone());
        BESS_DEBUG("(DigitalSimDriver.addComponent) Created component '{}' with UUID {} from definition '{}'",
                   comp->getName(), (uint64_t)comp->getUuid(), def->getName());

        if (!comp) {
            BESS_WARN("(DigitalSimDriver.addComponent) Failed to create component from definition: {}",
                      def->getName());
            return nullptr;
        }

        addComponent(comp);

        return comp;
    }
} // namespace Bess::SimEngine::Drivers::Digital

namespace Bess::JsonConvert {
    void toJsonValue(Json::Value &json,
                     const Bess::SimEngine::Drivers::Digital::DigitalSimComponent &data) {
        json = data.toJson();
    }
} // namespace Bess::JsonConvert
