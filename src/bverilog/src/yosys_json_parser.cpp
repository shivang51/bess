#include "bverilog/yosys_json_parser.h"
#include <json/value.h>
#include <set>
#include <stdexcept>

namespace Bess::Verilog {
    namespace {
        PortDirection parseDirection(const std::string &value) {
            if (value == "input") {
                return PortDirection::input;
            }
            if (value == "output") {
                return PortDirection::output;
            }
            if (value == "inout") {
                return PortDirection::inout;
            }
            throw std::runtime_error("Unsupported port direction in Yosys JSON: " + value);
        }

        SignalBit parseBit(const Json::Value &value) {
            if (value.isInt64()) {
                return SignalBit::fromNet(value.asInt64());
            }
            if (value.isUInt64()) {
                return SignalBit::fromNet(static_cast<int64_t>(value.asUInt64()));
            }
            if (value.isString()) {
                return SignalBit::fromConstant(value.asString());
            }
            throw std::runtime_error("Unsupported Yosys bit encoding");
        }

        std::vector<SignalBit> parseBits(const Json::Value &bitsJson) {
            if (!bitsJson.isArray()) {
                throw std::runtime_error("Expected Yosys bit vector array");
            }

            std::vector<SignalBit> bits;
            bits.reserve(bitsJson.size());
            for (const auto &bitJson : bitsJson) {
                bits.push_back(parseBit(bitJson));
            }
            return bits;
        }

        std::string pickTopModule(const std::vector<Module> &modules,
                                  const std::optional<std::string> &explicitTopModule) {
            if (explicitTopModule.has_value()) {
                return *explicitTopModule;
            }

            for (const auto &module : modules) {
                const auto attrIt = module.attributes.find("top");
                if (attrIt != module.attributes.end() &&
                    (attrIt->second == "1" || attrIt->second == "00000000000000000000000000000001")) {
                    return module.name;
                }
            }

            if (modules.size() == 1) {
                return modules.front().name;
            }

            std::set<std::string> referencedModules;
            for (const auto &module : modules) {
                for (const auto &cell : module.cells) {
                    referencedModules.insert(cell.type);
                }
            }

            std::vector<std::string> candidates;
            for (const auto &module : modules) {
                if (!referencedModules.contains(module.name)) {
                    candidates.push_back(module.name);
                }
            }

            if (candidates.size() == 1) {
                return candidates.front();
            }

            throw std::runtime_error("Unable to determine top module from Yosys JSON");
        }
    } // namespace

    SignalBit SignalBit::fromNet(int64_t bitId) {
        SignalBit bit;
        bit.netId = bitId;
        return bit;
    }

    SignalBit SignalBit::fromConstant(std::string value) {
        SignalBit bit;
        bit.constant = std::move(value);
        return bit;
    }

    bool SignalBit::isNet() const {
        return netId.has_value();
    }

    bool SignalBit::isConstant() const {
        return constant.has_value();
    }

    std::string SignalBit::toString() const {
        if (isNet()) {
            return std::to_string(*netId);
        }
        if (isConstant()) {
            return *constant;
        }
        return {};
    }

    const Port *Module::findPort(std::string_view portName) const {
        for (const auto &port : ports) {
            if (port.name == portName) {
                return &port;
            }
        }
        return nullptr;
    }

    const Module *Design::findModule(std::string_view moduleName) const {
        for (const auto &module : modules) {
            if (module.name == moduleName) {
                return &module;
            }
        }
        return nullptr;
    }

    Design parseDesignFromYosysJson(const Json::Value &root,
                                    const std::optional<std::string> &explicitTopModule) {
        if (!root.isObject() || !root.isMember("modules") || !root["modules"].isObject()) {
            throw std::runtime_error("Invalid Yosys JSON: missing modules object");
        }

        Design design;
        const auto &modulesJson = root["modules"];
        for (const auto &moduleName : modulesJson.getMemberNames()) {
            const auto &moduleJson = modulesJson[moduleName];

            Module module;
            module.name = moduleName;

            if (moduleJson.isMember("attributes") && moduleJson["attributes"].isObject()) {
                for (const auto &attrName : moduleJson["attributes"].getMemberNames()) {
                    module.attributes[attrName] = moduleJson["attributes"][attrName].asString();
                }
            }

            if (moduleJson.isMember("ports")) {
                for (const auto &portName : moduleJson["ports"].getMemberNames()) {
                    const auto &portJson = moduleJson["ports"][portName];
                    Port port;
                    port.name = portName;
                    port.direction = parseDirection(portJson["direction"].asString());
                    port.bits = parseBits(portJson["bits"]);
                    module.ports.push_back(std::move(port));
                }
            }

            if (moduleJson.isMember("cells")) {
                for (const auto &cellName : moduleJson["cells"].getMemberNames()) {
                    const auto &cellJson = moduleJson["cells"][cellName];
                    Cell cell;
                    cell.name = cellName;
                    cell.type = cellJson["type"].asString();

                    if (cellJson.isMember("connections")) {
                        for (const auto &connName : cellJson["connections"].getMemberNames()) {
                            cell.connections[connName] = parseBits(cellJson["connections"][connName]);
                        }
                    }

                    if (cellJson.isMember("port_directions")) {
                        for (const auto &dirName : cellJson["port_directions"].getMemberNames()) {
                            cell.portDirections[dirName] = parseDirection(cellJson["port_directions"][dirName].asString());
                        }
                    }

                    module.cells.push_back(std::move(cell));
                }
            }

            design.modules.push_back(std::move(module));
        }

        design.topModuleName = pickTopModule(design.modules, explicitTopModule);
        return design;
    }
} // namespace Bess::Verilog
