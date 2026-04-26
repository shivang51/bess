/**
 * @file expr_evaluator.h
 * @brief Expression evaluator for logic simulation engine
 * @details This file contains functions to evaluate logical expressions based on input slot states.
 * The expressions can include binary operators (+ for OR, * for AND, ^ for XOR)
 * and unary operators (! for NOT, $ for buffer).
 **/

#pragma once

#include <memory>
#include <string>
#include <vector>

namespace Bess::SimEngine::Drivers::Digital {
    class DigCompSimData;
} // namespace Bess::SimEngine::Drivers::Digital

namespace Bess::SimEngine::ExprEval {

    bool isUninaryOperator(char op);

    bool evaluateExpression(const std::string &expr, const std::vector<bool> &values);

    typedef std::shared_ptr<Drivers::Digital::DigCompSimData> TSimFnDataPtr;

    TSimFnDataPtr exprEvalSimFunc(const TSimFnDataPtr &simData);

} // namespace Bess::SimEngine::ExprEval
