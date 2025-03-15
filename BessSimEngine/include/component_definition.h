#pragma once

#include "component_types.h"
#include "properties.h"
#include "types.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace Bess::SimEngine {

    typedef std::unordered_map<Properties::ComponentProperty, std::vector<std::any>> ModifiableProperties;

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

        const ModifiableProperties &getModifiableProperties() const;

        ComponentDefinition &addModifiableProperty(Properties::ComponentProperty property, std::any value) {
            m_modifiableProperties[property].emplace_back(value);
            return *this;
        }

        ComponentDefinition &addModifiableProperty(Properties::ComponentProperty property, const std::vector<std::any> &value) {
            m_modifiableProperties[property] = value;
            return *this;
        }

      private:
        ModifiableProperties m_modifiableProperties = {};
    };

} // namespace Bess::SimEngine
