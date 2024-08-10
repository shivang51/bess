#include "simulator/simulator_engine.h"
#include <stdexcept>
#include <stack>

#include "components_manager/components_manager.h"
#include "components/input_probe.h"

namespace Bess::Simulator {
    int Engine::evaluateExpression(const std::string& expression, const std::vector<int>& arr)
    {
        std::stack<int> values;
        std::stack<char> operators;

        for (size_t i = 0; i < expression.size(); ++i) {
            char c = expression[i];

            if (isdigit(c)) {
                int index = c - '0'; // Convert char to int
                values.push(arr[index]);
            }
            else if (c == '(') {
                operators.push(c);
            }
            else if (c == ')') {
                while (!operators.empty() && operators.top() != '(') {
                    if (operators.top() == '!') {
                        int a = values.top(); values.pop();
                        char op = operators.top(); operators.pop();
                        values.push(applyOperator(op, a));
                    }
                    else {
                        int b = values.top(); values.pop();
                        int a = values.top(); values.pop();
                        char op = operators.top(); operators.pop();
                        values.push(applyOperator(op, a, b));
                    }
                }
                operators.pop(); // Remove '('
            }
            else if (c == '+' || c == '*' || c == '!') {
                while (!operators.empty() && operators.top() != '(' &&
                    ((c == '*' && (operators.top() == '*' || operators.top() == '!')) ||
                        (c == '+' && (operators.top() == '+' || operators.top() == '*' || operators.top() == '!')) ||
                        (c == '!' && operators.top() == '!'))) {
                    if (operators.top() == '!') {
                        int a = values.top(); values.pop();
                        char op = operators.top(); operators.pop();
                        values.push(applyOperator(op, a));
                    }
                    else {
                        int b = values.top(); values.pop();
                        int a = values.top(); values.pop();
                        char op = operators.top(); operators.pop();
                        values.push(applyOperator(op, a, b));
                    }
                }
                operators.push(c);
            }
        }

        while (!operators.empty()) {
            if (operators.top() == '!') {
                int a = values.top(); values.pop();
                char op = operators.top(); operators.pop();
                values.push(applyOperator(op, a));
            }
            else {
                int b = values.top(); values.pop();
                int a = values.top(); values.pop();
                char op = operators.top(); operators.pop();
                values.push(applyOperator(op, a, b));
            }
        }

        return values.top();
    }

    int Engine::applyOperator(char op, int a, int b)
    {
        switch (op) {
        case '+': return (a + b) >= 1 ? 1 : 0;
        case '*': return a * b;
        case '!': return a == 0 ? 1 : 0;
        default: throw std::invalid_argument("Invalid operator");
        }
    }

    void Engine::RefreshSimulation() {
        for (auto& comp : Simulator::ComponentsManager::components) {
            if (comp.second->getType() != ComponentType::inputProbe) continue;
            auto inpProbe = (Components::InputProbe*)comp.second.get();
            inpProbe->refresh();
        }
    }
}