/**
 * @file expr_evaluator.h
 * @brief Expression evaluator for logic simulation engine
 * @details This file contains functions to evaluate logical expressions based on input slot states.
 * The expressions can include binary operators (+ for OR, * for AND, ^ for XOR)
 * and unary operators (! for NOT, $ for buffer).
 **/

#pragma once

#include "types.h"
#include "common/logger.h"
#include <stack>
#include <stdexcept>
#include <vector>

namespace Bess::SimEngine::ExprEval {

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

    inline bool isUninaryOperator(char op) {
        return op == '!' || op == '$';
    }

    inline bool evaluateExpression(const std::string &expr, const std::vector<bool> &values) {
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

    /// expression evaluator simulation function
    const SimulationFunction exprEvalSimFunc = [](const std::vector<SlotState> &inputs, SimTime currentTime, const ComponentState &prevState) {
        auto newState = prevState;
        newState.inputStates = inputs;
        bool changed = false;
        assert(prevState.auxData && "ExprEvalSimFunc requires auxData to be set with expressions");
        if (prevState.auxData->type() != typeid(std::vector<std::string>)) {
            throw std::runtime_error(
                std::format(
                    "ExprEvalSimFunc auxData must be std::vector<std::string>, got {}",
                    prevState.auxData->type().name()));
        }
        auto expressions = std::any_cast<std::vector<std::string>>(prevState.auxData);
        for (int i = 0; i < (int)expressions->size(); i++) {
            std::vector<bool> states;
            states.reserve(inputs.size());
            for (auto &state : inputs)
                states.emplace_back((bool)state);
            bool newStateBool = ExprEval::evaluateExpression(expressions->at(i), states);
            changed = changed || (bool)prevState.outputStates[i] != newStateBool;
            newState.outputStates[i] = {newStateBool ? LogicState::high : LogicState::low, currentTime};
        }
        newState.isChanged = changed;
        return newState;
    };
} // namespace Bess::SimEngine::ExprEval
