#pragma once

#include <string>
#include <vector>

namespace Bess::Simulator {
    class Engine {
    public:
        static int evaluateExpression(const std::string& expression, const std::vector<int>& arr);
        static void RefreshSimulation();
    private:
        static int applyBinaryOperator(int a, int b, char op);
        static int applyUnaryOperator(int a, char op);
    };
}