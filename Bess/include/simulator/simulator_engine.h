#pragma once

#include <string>
#include <vector>

namespace Bess::Simulator {
    class Engine {
    public:
        static int evaluateExpression(const std::string& expression, const std::vector<int>& arr);
    private:
        static int applyOperator(char op, int a, int b = 0);
    };
}