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
        SimDelayMilliSeconds delay, const std::string& expr) {
        this->type = type;
        this->name = name;
        this->category = category;
        this->simulationFunction = simFunction;
        this->inputCount = inputCount;
        this->outputCount = outputCount;
        this->delay = delay;
        this->expression = expr;
    }

    const ModifiableProperties &ComponentDefinition::getModifiableProperties() const {
        return m_modifiableProperties;
    }

    std::string ComponentDefinition::getExpression(int n) const {
        if (op == '0') {
            return expression;
        }

        std::string expr = negate ? "!(0" : "0";
        for (size_t i = 1; i < n; i++) {
            expr += op + std::to_string(i);
        }
        if (negate)
            expr += ")";
        return expr;
    }
} // namespace Bess::SimEngine
