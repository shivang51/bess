#pragma once

#include "bess_api.h"
#include "component_types/component_types.h"
#include "properties.h"
#include "types.h"
#include <string>
#include <vector>

namespace Bess::SimEngine {

    class BESS_API ComponentDefinition {
      public:
        ComponentDefinition(const std::string &name,
                            const std::string &category,
                            int inputCount,
                            int outputCount,
                            SimulationFunction simFunction,
                            SimDelayNanoSeconds delay, char op);

        ComponentDefinition(const std::string &name,
                            const std::string &category,
                            int inputCount,
                            int outputCount,
                            SimulationFunction simFunction,
                            SimDelayNanoSeconds delay, const std::vector<std::string> &expr = {});

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
        std::any auxData;

        const Properties::ModifiableProperties &getModifiableProperties() const;

        ComponentDefinition &addModifiableProperty(Properties::ComponentProperty property, std::any value);

        ComponentDefinition &addModifiableProperty(Properties::ComponentProperty property, const std::vector<std::any> &value);

        std::vector<std::string> getExpressions(int inputCount = -1) const;

        std::pair<std::span<const PinDetails>, std::span<const PinDetails>> getPinDetails() const;

        uint64_t getHash() const noexcept;

        void invalidateHash() const;

        friend bool operator==(const ComponentDefinition &a, const ComponentDefinition &b) noexcept {
            return a.getHash() == b.getHash();
        }

      private:
        Properties::ModifiableProperties m_modifiableProperties = {};

        mutable uint64_t m_cachedHash = 0;
        mutable bool m_hashComputed = false;
    };

} // namespace Bess::SimEngine
