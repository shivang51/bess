#include "expression_evalutator/expr_evaluator.h"
#include "common/bess_assert.h"

#include "common/logger.h"
#include "types.h"

#include <stack>
#include <stdexcept>

namespace Bess::SimEngine::ExprEval {
    inline bool isUninaryOperator(char op) { return op == '!' || op == '$'; }

    inline bool applyBinaryOperator(bool a, bool b, char op) {
        switch (op) {
        case '+':
            return a || b;
        case '*':
            return a && b;
        case '^':
            return a ^ b;
        default:
            throw std::runtime_error("Unsupported binary operator");
        }
    }

    inline bool applyUnaryOperator(bool a, char op) {
        if (op == '!')
            return !a;
        else if (op == '$') // buffer operator
            return a;
        throw std::runtime_error("Unsupported unary operator");
    }

    bool evaluateExpression(const std::string &expr,
                            const std::vector<bool> &values) {
        std::stack<bool> operands;
        std::stack<char> operators;

        auto precedence = [](char op) {
            switch (op) {
            case '$':
                return 5;
            case '!':
                return 4;
            case '*':
                return 3;
            case '^':
                return 2;
            case '+':
                return 1;
            default:
                return 0;
            }
        };

        auto applyTopOperator = [&]() {
            char op = operators.top();
            operators.pop();
            if (op == '!' || op == '$') {
                int a = operands.top();
                operands.pop();
                operands.push(applyUnaryOperator(a, op));
            } else {
                int b = operands.top();
                operands.pop();
                int a = operands.top();
                operands.pop();
                operands.push(applyBinaryOperator(a, b, op));
            }
        };

        for (size_t i = 0; i < expr.size(); ++i) {
            if (isspace(expr[i]))
                continue; // Skip whitespaces

            if (isdigit(expr[i])) {
                int index = expr[i] - '0';
                if (index >= values.size()) {
                    BESS_ERROR("Invalid index {} in epxr {}", index, expr);
                    throw std::out_of_range(
                        "Index out of range in the values array");
                }
                operands.push(values[index]);
            } else if (expr[i] == '(') {
                operators.push('(');
            } else if (expr[i] == ')') {
                while (!operators.empty() && operators.top() != '(') {
                    applyTopOperator();
                }
                operators.pop(); // Remove '('
            } else if (expr[i] == '+' || expr[i] == '*' || expr[i] == '^') {
                while (!operators.empty() &&
                       precedence(operators.top()) >= precedence(expr[i])) {
                    applyTopOperator();
                }
                operators.push(expr[i]);
            } else if (isUninaryOperator(expr[i])) {
                operators.push(expr[i]);
            } else {
                BESS_ERROR("Invalid expression {}", expr);
                throw std::runtime_error("Invalid character in expression");
            }
        }

        // Apply remaining operators
        while (!operators.empty()) {
            applyTopOperator();
        }

        return operands.top();
    }
} // namespace Bess::SimEngine::ExprEval
