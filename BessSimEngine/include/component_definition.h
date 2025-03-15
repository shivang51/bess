#pragma once

#include "component_types.h"
#include "types.h"
#include <string>

namespace Bess::SimEngine {
    class ComponentDefinition {
      public:
        ComponentDefinition(ComponentType type,
                            const std::string &name,
                            const std::string &category,
                            int inputCount,
                            int outputCount,
                            SimulationFunction simFunction,
                            SimDelayMilliSeconds delay);

        ComponentType type;
        std::string name;
        std::string category;
        SimDelayMilliSeconds delay;
        SimulationFunction simulationFunction;
        int inputCount;
        int outputCount;
    };

} // namespace Bess::SimEngine
