#pragma once

#include "bess_api.h"
#include "component_types/component_types.h"
#include "properties.h"
#include "spdlog/fmt/bundled/base.h"
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
                            SimDelayNanoSeconds delay, char op);

        ComponentDefinition(ComponentType type,
                            const std::string &name,
                            const std::string &category,
                            int inputCount,
                            int outputCount,
                            SimulationFunction simFunction,
                            SimDelayNanoSeconds delay, const std::vector<std::string> &expr = {});

        ComponentType type;
        std::string name;
        std::string category;
        SimDelayNanoSeconds delay, setupTime, holdTime;
        SimulationFunction simulationFunction;
        std::vector<std::string> expressions = {};
        std::vector<PinDetails> inputPinDetails = {};
        std::vector<PinDetails> outputPinDetails = {};
        int inputCount;
        int outputCount;
        char op = '0';
        bool negate = false;

        const ModifiableProperties &getModifiableProperties() const;

        ComponentDefinition &addModifiableProperty(Properties::ComponentProperty property, std::any value);

        ComponentDefinition &addModifiableProperty(Properties::ComponentProperty property, const std::vector<std::any> &value);

        std::vector<std::string> getExpressions(int inputCount = -1) const;

        std::pair<std::span<const PinDetails>, std::span<const PinDetails>> getPinDetails() const;

      private:
        ModifiableProperties m_modifiableProperties = {};
    };

} // namespace Bess::SimEngine
