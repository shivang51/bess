#pragma once

#include "bess_api.h"
#include "component_types.h"
#include "properties.h"
#include "types.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace Bess::SimEngine {

    typedef std::unordered_map<Properties::ComponentProperty, std::vector<std::any>> ModifiableProperties;

    class BESS_API ComponentDefinition {
      public:
        ComponentDefinition(ComponentType type,
                            const std::string &name,
                            const std::string &category,
                            int inputCount,
                            int outputCount,
                            SimulationFunction simFunction,
                            SimDelayMilliSeconds delay, char op);

        ComponentDefinition(ComponentType type,
                            const std::string &name,
                            const std::string &category,
                            int inputCount,
                            int outputCount,
                            SimulationFunction simFunction,
                            SimDelayMilliSeconds delay, const std::string &expr = "");

        ComponentType type;
        std::string name;
        std::string category;
        SimDelayMilliSeconds delay;
        SimulationFunction simulationFunction;
        int inputCount;
        int outputCount;
        char op = '0';
        std::string expression = "";
        bool negate = false;

        const ModifiableProperties &getModifiableProperties() const;

        ComponentDefinition &addModifiableProperty(Properties::ComponentProperty property, std::any value) {
            m_modifiableProperties[property].emplace_back(value);
            return *this;
        }

        ComponentDefinition &addModifiableProperty(Properties::ComponentProperty property, const std::vector<std::any> &value) {
            m_modifiableProperties[property] = value;
            return *this;
        }

        std::string getExpression(int n = -1) const;
      private:
        ModifiableProperties m_modifiableProperties = {};
    };

} // namespace Bess::SimEngine
