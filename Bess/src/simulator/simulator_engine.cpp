#include "simulator/simulator_engine.h"
#include <stdexcept>
#include <stack>

#include "components_manager/components_manager.h"
#include "components/input_probe.h"

namespace Bess::Simulator {
    // Function to apply a binary operator to two operands
    int Engine::applyBinaryOperator(int a, int b, char op) {
        switch (op) {
        case '+': return (a + b) >= 1 ? 1 : 0;
        case '*': return a * b;
        case '^': return a ^ b;
        default: throw std::runtime_error("Unsupported binary operator");
        }
    }

    // Function to apply the unary NOT operator
    int Engine::applyUnaryOperator(int a, char op) {
        if (op == '!') return !a;
        throw std::runtime_error("Unsupported unary operator");
    }

    // Function to evaluate the expression
    int Engine::evaluateExpression(const std::string& expr, const std::vector<int>& values) {
        std::stack<int> operands;
        std::stack<char> operators;

        auto precedence = [](char op) {
            switch (op) {
            case '!': return 3;
            case '*': return 2;
            case '^': return 2;
            case '+': return 1;
            default: return 0;
            }
            };

        auto applyTopOperator = [&]() {
            char op = operators.top(); operators.pop();
            if (op == '!') {
                int a = operands.top(); operands.pop();
                operands.push(applyUnaryOperator(a, op));
            }
            else {
                int b = operands.top(); operands.pop();
                int a = operands.top(); operands.pop();
                operands.push(applyBinaryOperator(a, b, op));
            }
            };

        for (size_t i = 0; i < expr.size(); ++i) {
            if (isspace(expr[i])) continue;  // Skip whitespaces

            if (isdigit(expr[i])) {
                int index = expr[i] - '0';
                if (index >= values.size()) {
                    throw std::out_of_range("Index out of range in the values array");
                }
                operands.push(values[index]);
            }
            else if (expr[i] == '(') {
                operators.push('(');
            }
            else if (expr[i] == ')') {
                while (!operators.empty() && operators.top() != '(') {
                    applyTopOperator();
                }
                operators.pop(); // Remove '('
            }
            else if (expr[i] == '+' || expr[i] == '*' || expr[i] == '^') {
                while (!operators.empty() && precedence(operators.top()) >= precedence(expr[i])) {
                    applyTopOperator();
                }
                operators.push(expr[i]);
            }
            else if (expr[i] == '!') {
                operators.push('!');
            }
            else {
                throw std::runtime_error("Invalid character in expression");
            }
        }

        // Apply remaining operators
        while (!operators.empty()) {
            applyTopOperator();
        }

        return operands.top();
    }

    void Engine::RefreshSimulation() {
        for (auto& comp : Simulator::ComponentsManager::components) {
            if (comp.second->getType() != ComponentType::inputProbe) continue;
            auto inpProbe = (Components::InputProbe*)comp.second.get();
            inpProbe->refresh();
        }
    }
}