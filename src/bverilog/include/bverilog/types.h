#pragma once

#include "bess_api.h"
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Bess::Verilog {
    enum class PortDirection : uint8_t {
        input,
        output,
        inout
    };

    struct BESS_API SignalBit {
        std::optional<int64_t> netId;
        std::optional<std::string> constant;

        static SignalBit fromNet(int64_t bitId);
        static SignalBit fromConstant(std::string value);

        bool isNet() const;
        bool isConstant() const;
        std::string toString() const;
    };

    struct BESS_API Port {
        std::string name;
        PortDirection direction = PortDirection::input;
        std::vector<SignalBit> bits;
    };

    struct BESS_API Cell {
        std::string name;
        std::string type;
        std::unordered_map<std::string, std::vector<SignalBit>> connections;
        std::unordered_map<std::string, PortDirection> portDirections;
    };

    struct BESS_API Module {
        std::string name;
        std::vector<Port> ports;
        std::vector<Cell> cells;
        std::unordered_map<std::string, std::string> attributes;

        const Port *findPort(std::string_view portName) const;
    };

    struct BESS_API Design {
        std::vector<Module> modules;
        std::string topModuleName;

        const Module *findModule(std::string_view moduleName) const;
    };
} // namespace Bess::Verilog
