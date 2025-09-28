#pragma once
#include <stdexcept>
#include <stack>
#include <vector>
#include <logger.h>

namespace Bess::SimEngine::ExprEval {
    inline bool applyBinaryOperator(const bool a, const bool b, const char op) {
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

    inline bool applyUnaryOperator(const bool a, const char op) {
        if (op == '!')
            return !a;
        throw std::runtime_error("Unsupported unary operator");
    }

    inline bool evaluateExpression(const std::string &expr, const std::vector<bool> &values) {
        std::stack<bool> operands;
        std::stack<char> operators;

        auto precedence = [](const char op) {
            switch (op) {
            case '!':
                return 3;
            case '*':
                return 2;
            case '^':
                return 2;
            case '+':
                return 1;
            default:
                return 0;
            }
        };

        auto applyTopOperator = [&]() {
            const char op = operators.top();
            operators.pop();
            if (op == '!') {
                const int a = operands.top();
                operands.pop();
                operands.push(applyUnaryOperator(a, op));
            } else {
                const int b = operands.top();
                operands.pop();
                const int a = operands.top();
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
                    BESS_SE_ERROR("Invalid index {} in epxr {}", index, expr);
                    throw std::out_of_range("Index out of range in the values array");
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
                while (!operators.empty() && precedence(operators.top()) >= precedence(expr[i])) {
                    applyTopOperator();
                }
                operators.push(expr[i]);
            } else if (expr[i] == '!') {
                operators.push('!');
            } else {
                BESS_SE_ERROR("Invalid expression {}", expr);
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