#include "component_definition.h"

namespace Bess::SimEngine {
    ComponentDefinition::ComponentDefinition(
        ComponentType type,
        const std::string &name,
        const std::string &category,
        int inputCount,
        int outputCount,
        SimulationFunction simFunction,
        SimDelayMilliSeconds delay) {
        this->type = type;
        this->name = name;
        this->category = category;
        this->simulationFunction = simFunction;
        this->inputCount = inputCount;
        this->outputCount = outputCount;
        this->delay = delay;
    }
} // namespace Bess::SimEngine
