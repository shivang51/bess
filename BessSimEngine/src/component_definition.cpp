#include "component_definition.h"
#include <logger.h>

namespace Bess::SimEngine {
    ComponentDefinition::ComponentDefinition(
        ComponentType type,
        const std::string &name,
        const std::string &category,
        int inputCount, int outputCount,
        SimulationFunction simFunction,
        SimDelayMilliSeconds delay, char op) {
        this->type = type;
        this->name = name;
        this->category = category;
        this->simulationFunction = simFunction;
        this->inputCount = inputCount;
        this->outputCount = outputCount;
        this->delay = delay;
        this->op = op;
    }

    ComponentDefinition::ComponentDefinition(
        ComponentType type,
        const std::string &name,
        const std::string &category,
        int inputCount, int outputCount,
        SimulationFunction simFunction,
        SimDelayMilliSeconds delay, const std::vector<std::string>& expr) {
        this->type = type;
        this->name = name;
        this->category = category;
        this->simulationFunction = simFunction;
        this->inputCount = inputCount;
        this->outputCount = outputCount;
        this->delay = delay;
        this->expressions = expr;
    }

    const ModifiableProperties &ComponentDefinition::getModifiableProperties() const {
        return m_modifiableProperties;
    }

    std::vector<std::string> ComponentDefinition::getExpressions(int n) const {
        if (op == '0') {
            return expressions;
        }

        // non unirary expressions where inputs are greater than 1 and output is just single value
        if (inputCount != 1 && outputCount == 1) {
            std::string expr = negate ? "!(0" : "0";
            for (size_t i = 1; i < n; i++) {
                expr += op + std::to_string(i);
            }
            if (negate)
                expr += ")";
            return {expr};
        } else if (inputCount == outputCount) {
            std::vector<std::string> expr;
            for (int i = 0; i < n; i++) {
                expr.emplace_back(std::format("{}{}", op, i));
            }
            return expr;
        }

        BESS_SE_ERROR("Invalid IO config for expression generation");
        assert(false);
    }
} // namespace Bess::SimEngine
