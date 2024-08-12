#pragma once

#include <string>
#include <vector>
#include <queue>
#include "uuid_v4.h"
#include "common/digital_state.h"

namespace Bess::Simulator {
    class Engine {
    public:
        struct SimQueueElement{
            UUIDv4::UUID uid;
            UUIDv4::UUID changerId;
            DigitalState state;
        };

        static int evaluateExpression(const std::string& expression, const std::vector<int>& arr);
        static void RefreshSimulation();
        static void Simulate();
        static void addToSimQueue(const UUIDv4::UUID& uid, const UUIDv4::UUID& changerId, Simulator::DigitalState state);
        static void clearQueue();
    private:
        static int applyBinaryOperator(int a, int b, char op);
        static int applyUnaryOperator(int a, char op);

        static std::queue<SimQueueElement> currentSimQueue, nextSimQueue;

    };
}