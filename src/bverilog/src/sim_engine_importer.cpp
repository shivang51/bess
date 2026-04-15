#include "bverilog/sim_engine_importer.h"
#include "common/bess_assert.h"
#include "common/logger.h"
#include "component_catalog.h"
#include "component_definition.h"
#include "digital_component.h"
#include "expression_evalutator/expr_evaluator.h"
#include "init_components.h"
#include "types.h"
#include <fstream>
#include <algorithm>
#include <cctype>
#include <memory>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace Bess::Verilog {
    using namespace Bess::SimEngine;

    namespace {
        std::string parentInstancePath(std::string_view path) {
            const auto separator = path.rfind('/');
            if (separator == std::string_view::npos) {
                return {};
            }
            return std::string(path.substr(0, separator));
        }

        std::string trim(std::string value) {
            const auto first = value.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) {
                return {};
            }
            const auto last = value.find_last_not_of(" \t\r\n");
            return value.substr(first, last - first + 1);
        }

        std::vector<std::string> tryReadModulePortOrderFromSource(const Module &module) {
            const auto srcIt = module.attributes.find("src");
            if (srcIt == module.attributes.end()) {
                return {};
            }

            const auto colon = srcIt->second.rfind(':');
            if (colon == std::string::npos) {
                return {};
            }

            const auto filePath = srcIt->second.substr(0, colon);
            const auto range = srcIt->second.substr(colon + 1);
            const auto lineSep = range.find('.');
            if (lineSep == std::string::npos) {
                return {};
            }

            const auto startLine = std::stoul(range.substr(0, lineSep));

            std::ifstream source(filePath);
            if (!source.is_open()) {
                return {};
            }

            std::string line;
            std::string header;
            size_t currentLine = 0;
            bool collecting = false;
            while (std::getline(source, line)) {
                ++currentLine;
                if (!collecting && currentLine >= startLine &&
                    line.find("module " + module.name) != std::string::npos) {
                    collecting = true;
                }

                if (!collecting) {
                    continue;
                }

                header += line;
                header.push_back('\n');
                if (line.find(");") != std::string::npos) {
                    break;
                }
            }

            const auto modulePos = header.find("module " + module.name);
            if (modulePos == std::string::npos) {
                return {};
            }

            const auto openParen = header.find('(', modulePos);
            const auto closeParen = header.find(')', openParen);
            if (openParen == std::string::npos || closeParen == std::string::npos || closeParen <= openParen) {
                return {};
            }

            std::vector<std::string> orderedNames;
            std::stringstream portList(header.substr(openParen + 1, closeParen - openParen - 1));
            std::string token;
            while (std::getline(portList, token, ',')) {
                token = trim(token);
                if (!token.empty()) {
                    orderedNames.push_back(token);
                }
            }
            return orderedNames;
        }

        std::vector<std::string> buildPortSlotNames(const Module &module, PortDirection direction) {
            std::vector<std::string> slotNames;
            std::unordered_set<std::string> seenPorts;

            auto appendPortBits = [&](const Port &port) {
                seenPorts.insert(port.name);
                if (port.direction != direction) {
                    return;
                }

                if (port.bits.size() == 1) {
                    slotNames.push_back(port.name);
                    return;
                }

                for (size_t i = 0; i < port.bits.size(); ++i) {
                    slotNames.push_back(port.name + "[" + std::to_string(i) + "]");
                }
            };

            for (const auto &portName : tryReadModulePortOrderFromSource(module)) {
                const auto *port = module.findPort(portName);
                if (!port) {
                    continue;
                }
                appendPortBits(*port);
            }

            for (const auto &port : module.ports) {
                if (seenPorts.contains(port.name)) {
                    continue;
                }
                appendPortBits(port);
            }
            return slotNames;
        }

        struct SlotEndpoint {
            UUID componentId = UUID::null;
            SlotType slotType = SlotType::digitalInput;
            int slotIndex = 0;
        };

        ImportedSlotEndpoint toImportedSlotEndpoint(const SlotEndpoint &endpoint) {
            return ImportedSlotEndpoint{
                .componentId = endpoint.componentId,
                .slotType = endpoint.slotType,
                .slotIndex = endpoint.slotIndex,
            };
        }

        std::vector<const Port *> orderedPortsForDirection(const Module &module, PortDirection direction) {
            std::vector<const Port *> orderedPorts;
            std::unordered_set<std::string> seenPorts;

            for (const auto &portName : tryReadModulePortOrderFromSource(module)) {
                const auto *port = module.findPort(portName);
                if (!port || port->direction != direction) {
                    continue;
                }
                orderedPorts.push_back(port);
                seenPorts.insert(port->name);
            }

            for (const auto &port : module.ports) {
                if (port.direction != direction || seenPorts.contains(port.name)) {
                    continue;
                }
                orderedPorts.push_back(&port);
            }

            return orderedPorts;
        }

        struct ModuleInstantiation {
            ImportedModuleInstance instance;
        };

        enum class SignalRefKind : uint8_t {
            net,
            constant,
            endpoint
        };

        struct SignalRef {
            SignalRefKind kind = SignalRefKind::net;
            std::string netKey;
            std::string constant;
            SlotEndpoint endpoint;

            static SignalRef net(std::string key) {
                return SignalRef{SignalRefKind::net, std::move(key), {}, {}};
            }

            static SignalRef constantValue(std::string value) {
                return SignalRef{SignalRefKind::constant, {}, std::move(value), {}};
            }

            static SignalRef endpointValue(SlotEndpoint endpoint) {
                SignalRef value;
                value.kind = SignalRefKind::endpoint;
                value.endpoint = endpoint;
                return value;
            }
        };

        std::shared_ptr<ComponentDefinition> findDefinitionByName(std::string_view name) {
            const auto &components = ComponentCatalog::instance().getComponents();
            for (const auto &component : components) {
                if (component && component->getName() == name) {
                    return component;
                }
            }
            return nullptr;
        }

        void resizeOutputs(const std::shared_ptr<DigitalComponent> &component, size_t count) {
            while (component->definition->getOutputSlotsInfo().count < count) {
                component->incrementOutputCount(true);
            }
            while (component->definition->getOutputSlotsInfo().count > count &&
                   component->definition->getOutputSlotsInfo().count > 1) {
                component->decrementOutputCount(true);
            }
        }

        void resizeInputs(const std::shared_ptr<DigitalComponent> &component, size_t count) {
            while (component->definition->getInputSlotsInfo().count < count) {
                component->incrementInputCount(true);
            }
            while (component->definition->getInputSlotsInfo().count > count &&
                   component->definition->getInputSlotsInfo().count > 1) {
                component->decrementInputCount(true);
            }
        }

        std::shared_ptr<ComponentDefinition> ensureBuiltinIoDefinition(const std::string &name) {
            auto definition = findDefinitionByName(name);
            if (definition) {
                return definition;
            }
            initIO();
            definition = findDefinitionByName(name);
            if (!definition) {
                throw std::runtime_error("Required built-in definition is missing from catalog: " + name);
            }
            return definition;
        }

        std::shared_ptr<ComponentDefinition> ensureExprDefinition(const std::string &name,
                                                                  size_t inputs,
                                                                  size_t outputs,
                                                                  const std::vector<std::string> &expressions) {
            auto definition = findDefinitionByName(name);
            if (definition) {
                return definition;
            }

            BESS_TRACE("Creating new component definition for %s with %zu inputs and %zu outputs",
                       name.c_str(), inputs, outputs);

            auto created = std::make_shared<ComponentDefinition>();
            created->setName(name);
            created->setGroupName("Verilog Imported");
            created->setInputSlotsInfo({SlotsGroupType::input, false, inputs, {}, {}});
            created->setOutputSlotsInfo({SlotsGroupType::output, false, outputs, {}, {}});
            created->setOutputExpressions(expressions);
            created->setSimulationFunction(ExprEval::exprEvalSimFunc);
            created->setSimDelay(SimDelayNanoSeconds(2));

            ComponentCatalog::instance().registerComponent(created);
            definition = findDefinitionByName(name);
            if (!definition) {
                throw std::runtime_error("Failed to register component definition: " + name);
            }
            return definition;
        }

        struct DffParams {
            bool risingEdge = true;
            bool hasReset = false;
            bool resetActiveHigh = true;
            bool resetToOne = false;
            bool asyncReset = true;
            bool hasEnable = false;
            bool enableActiveHigh = true;

            // Input slot indices depend on which features are present.
            // D is always 0, CLK is always 1.
            int rstSlotIndex() const { return hasReset ? 2 : -1; }
            int enSlotIndex() const {
                if (!hasEnable)
                    return -1;
                return hasReset ? 3 : 2;
            }
            size_t inputCount() const {
                size_t n = 2; // D + CLK
                if (hasReset)
                    ++n;
                if (hasEnable)
                    ++n;
                return n;
            }
        };

        std::optional<DffParams> parseDffCellType(const std::string &cellType) {
            // Handle coarse-grain Yosys cells
            if (cellType == "$dff")
                return DffParams{true, false, true, false, true, false, true};
            if (cellType == "$dffe")
                return DffParams{true, false, true, false, true, true, true};
            if (cellType == "$adff")
                return DffParams{true, true, true, false, true, false, true};
            if (cellType == "$sdff")
                return DffParams{true, true, true, false, false, false, true};

            // Parse $_DFF_*, $_DFFE_*, $_SDFF_*, $_SDFFE_* naming convention
            if (cellType.size() < 7 || cellType.front() != '$' || cellType.back() != '_') {
                return std::nullopt;
            }

            std::string_view sv(cellType);
            sv.remove_prefix(2); // skip "$_"
            sv.remove_suffix(1); // skip trailing "_"

            bool isSyncReset = false;
            bool isEnableVariant = false;

            if (sv.starts_with("SDFFE_")) {
                isSyncReset = true;
                isEnableVariant = true;
                sv.remove_prefix(6);
            } else if (sv.starts_with("SDFF_")) {
                isSyncReset = true;
                sv.remove_prefix(5);
            } else if (sv.starts_with("DFFE_")) {
                isEnableVariant = true;
                sv.remove_prefix(5);
            } else if (sv.starts_with("DFF_")) {
                sv.remove_prefix(4);
            } else {
                return std::nullopt;
            }

            // sv now holds the polarity/value suffix, e.g. "P", "PP0", "PN0P"
            if (sv.empty())
                return std::nullopt;

            DffParams params;
            params.asyncReset = !isSyncReset;

            // First char: clock polarity
            if (sv[0] == 'P')
                params.risingEdge = true;
            else if (sv[0] == 'N')
                params.risingEdge = false;
            else
                return std::nullopt;
            sv.remove_prefix(1);

            if (sv.empty()) {
                // $_DFF_P_ or $_DFF_N_ (basic, no reset, no enable)
                if (isEnableVariant)
                    return std::nullopt; // $_DFFE_P_ needs at least enable polarity
                return params;
            }

            if (isEnableVariant && sv.size() == 1) {
                // $_DFFE_PP_, $_DFFE_PN_, etc. — enable only, no reset
                if (sv[0] == 'P')
                    params.enableActiveHigh = true;
                else if (sv[0] == 'N')
                    params.enableActiveHigh = false;
                else
                    return std::nullopt;
                params.hasEnable = true;
                return params;
            }

            // Reset polarity
            if (sv[0] == 'P')
                params.resetActiveHigh = true;
            else if (sv[0] == 'N')
                params.resetActiveHigh = false;
            else
                return std::nullopt;
            sv.remove_prefix(1);

            if (sv.empty())
                return std::nullopt; // need reset value

            // Reset value
            if (sv[0] == '0')
                params.resetToOne = false;
            else if (sv[0] == '1')
                params.resetToOne = true;
            else
                return std::nullopt;
            sv.remove_prefix(1);

            params.hasReset = true;

            if (isEnableVariant) {
                if (sv.size() != 1)
                    return std::nullopt;
                if (sv[0] == 'P')
                    params.enableActiveHigh = true;
                else if (sv[0] == 'N')
                    params.enableActiveHigh = false;
                else
                    return std::nullopt;
                params.hasEnable = true;
            } else {
                if (!sv.empty())
                    return std::nullopt;
            }

            return params;
        }

        std::string dffDefinitionName(const DffParams &p) {
            std::string name = "D Flip Flop";
            if (!p.risingEdge)
                name += " (Neg Edge)";
            if (p.hasReset) {
                name += p.asyncReset ? " A" : " S";
                name += p.resetActiveHigh ? "R+" : "R-";
                name += p.resetToOne ? "1" : "0";
            }
            if (p.hasEnable) {
                name += p.enableActiveHigh ? " EN+" : " EN-";
            }
            return name;
        }

        std::shared_ptr<ComponentDefinition> ensureGeneralDffDefinition(const DffParams &p) {
            const auto name = dffDefinitionName(p);
            auto definition = findDefinitionByName(name);
            if (definition) {
                return definition;
            }

            auto created = std::make_shared<ComponentDefinition>();
            created->setName(name);
            created->setGroupName("Verilog Imported");

            std::vector<std::string> inputNames = {"D", "CLK"};
            std::vector<std::pair<int, SlotCatergory>> categories = {
                {1, SlotCatergory::clock},
            };
            if (p.hasReset) {
                inputNames.push_back("RST");
                categories.push_back({static_cast<int>(inputNames.size() - 1), SlotCatergory::clear});
            }
            if (p.hasEnable) {
                inputNames.push_back("EN");
                categories.push_back({static_cast<int>(inputNames.size() - 1), SlotCatergory::enable});
            }

            SlotsGroupInfo inputs{SlotsGroupType::input, false, inputNames.size(), inputNames, {}};
            inputs.categories = categories;
            created->setInputSlotsInfo(inputs);
            created->setOutputSlotsInfo({SlotsGroupType::output, false, 2, {"Q", "Q'"}, {}});
            created->setSimDelay(SimDelayNanoSeconds(2));
            auto paramsToJson = [p]() {
                Json::Value j;
                j["risingEdge"] = p.risingEdge;
                j["hasReset"] = p.hasReset;
                j["resetActiveHigh"] = p.resetActiveHigh;
                j["resetToOne"] = p.resetToOne;
                j["asyncReset"] = p.asyncReset;
                j["hasEnable"] = p.hasEnable;
                j["enableActiveHigh"] = p.enableActiveHigh;
                return j;
            };

            created->setAuxData(VerCompDefAuxData{
                .id = "DffParams",
                .toJsonCb = paramsToJson,
            });

            const auto rstIdx = p.rstSlotIndex();
            const auto enIdx = p.enSlotIndex();

            created->setSimulationFunction(
                [p, rstIdx, enIdx](const std::vector<SlotState> &inputs,
                                   SimTime simTime,
                                   const ComponentState &prevState) -> ComponentState {
                    auto next = prevState;
                    next.inputStates = inputs;
                    next.isChanged = false;

                    const auto currentClock = inputs[1].state == LogicState::high;
                    const auto previousClock = prevState.inputStates.size() > 1 &&
                                               prevState.inputStates[1].state == LogicState::high;
                    const bool clockEdge = p.risingEdge
                                               ? (!previousClock && currentClock)
                                               : (previousClock && !currentClock);

                    // Check enable (if present)
                    if (p.hasEnable && enIdx >= 0 && enIdx < static_cast<int>(inputs.size())) {
                        const bool enActive = inputs[enIdx].state == LogicState::high;
                        const bool enabled = p.enableActiveHigh ? enActive : !enActive;
                        if (!enabled && !p.hasReset) {
                            // No reset to check, and not enabled — nothing happens
                            return next;
                        }
                        if (!enabled) {
                            // Still need to check async reset below, but clock-triggered
                            // data capture is suppressed.
                        }
                    }

                    // Check reset
                    bool resetActive = false;
                    if (p.hasReset && rstIdx >= 0 && rstIdx < static_cast<int>(inputs.size())) {
                        const bool rstHigh = inputs[rstIdx].state == LogicState::high;
                        resetActive = p.resetActiveHigh ? rstHigh : !rstHigh;
                    }

                    const auto resetValue = p.resetToOne ? LogicState::high : LogicState::low;
                    SlotState q = prevState.outputStates.empty() ? SlotState{} : prevState.outputStates[0];

                    if (p.asyncReset && resetActive) {
                        // Async reset: applied regardless of clock
                        q.state = resetValue;
                        q.lastChangeTime = simTime;
                    } else if (clockEdge) {
                        // Check enable gate for data capture
                        bool enabled = true;
                        if (p.hasEnable && enIdx >= 0 && enIdx < static_cast<int>(inputs.size())) {
                            const bool enActive = inputs[enIdx].state == LogicState::high;
                            enabled = p.enableActiveHigh ? enActive : !enActive;
                        }

                        if (!p.asyncReset && resetActive) {
                            // Sync reset: applied on clock edge
                            q.state = resetValue;
                            q.lastChangeTime = simTime;
                        } else if (enabled) {
                            q = inputs[0];
                            q.lastChangeTime = simTime;
                        } else {
                            return next;
                        }
                    } else {
                        return next;
                    }

                    next.outputStates[0] = q;
                    auto qInv = q;
                    if (qInv.state == LogicState::high) {
                        qInv.state = LogicState::low;
                    } else if (qInv.state == LogicState::low) {
                        qInv.state = LogicState::high;
                    }
                    next.outputStates[1] = qInv;
                    next.isChanged = prevState.outputStates.empty() ||
                                     prevState.outputStates[0].state != next.outputStates[0].state;
                    return next;
                });

            ComponentCatalog::instance().registerComponent(created);
            definition = findDefinitionByName(name);
            if (!definition) {
                throw std::runtime_error("Failed to register DFF definition: " + name);
            }
            return definition;
        }

        std::shared_ptr<ComponentDefinition> resolvePrimitiveDefinition(const std::string &cellType) {
            if (cellType == "$_BUF_" || cellType == "$buf") {
                return ensureExprDefinition("Buffer Gate", 1, 1, {"0"});
            }
            if (cellType == "$_NOT_" || cellType == "$not" || cellType == "$logic_not") {
                return ensureExprDefinition("NOT Gate", 1, 1, {"!0"});
            }
            if (cellType == "$_AND_" || cellType == "$and") {
                return ensureExprDefinition("AND Gate", 2, 1, {"0*1"});
            }
            if (cellType == "$_NAND_" || cellType == "$nand") {
                return ensureExprDefinition("NAND Gate", 2, 1, {"!(0*1)"});
            }
            if (cellType == "$_OR_" || cellType == "$or") {
                return ensureExprDefinition("OR Gate", 2, 1, {"0+1"});
            }
            if (cellType == "$_NOR_" || cellType == "$nor") {
                return ensureExprDefinition("NOR Gate", 2, 1, {"!(0+1)"});
            }
            if (cellType == "$_XOR_" || cellType == "$xor") {
                return ensureExprDefinition("XOR Gate", 2, 1, {"0^1"});
            }
            if (cellType == "$_XNOR_" || cellType == "$xnor") {
                return ensureExprDefinition("XNOR Gate", 2, 1, {"!(0^1)"});
            }
            if (cellType == "$reduce_and") {
                return ensureExprDefinition("Reduction AND", 2, 1, {"0*1"});
            }
            if (cellType == "$reduce_or" || cellType == "$reduce_bool") {
                return ensureExprDefinition("Reduction OR", 2, 1, {"0+1"});
            }
            if (cellType == "$reduce_xor") {
                return ensureExprDefinition("Reduction XOR", 2, 1, {"0^1"});
            }
            if (cellType == "$reduce_xnor") {
                return ensureExprDefinition("Reduction XNOR", 2, 1, {"!(0^1)"});
            }
            if (cellType == "$_MUX_" || cellType == "$mux") {
                return ensureExprDefinition("2-to-1 Multiplexer", 3, 1, {"(!2*0) + (2*1)"});
            }
            const auto dffParams = parseDffCellType(cellType);
            if (dffParams.has_value()) {
                return ensureGeneralDffDefinition(*dffParams);
            }
            return nullptr;
        }

        LogicState constantToLogicState(const std::string &constant) {
            if (constant == "0") {
                return LogicState::low;
            }
            if (constant == "1") {
                return LogicState::high;
            }
            if (constant == "x") {
                return LogicState::unknown;
            }
            if (constant == "z") {
                return LogicState::high_z;
            }
            throw std::runtime_error("Unsupported constant bit in imported Verilog: " + constant);
        }

        uint64_t parseYosysBinaryUnsigned(std::string_view value) {
            if (value.empty()) {
                return 0;
            }

            uint64_t parsed = 0;
            const size_t start = value.size() > 64 ? value.size() - 64 : 0;
            for (size_t i = start; i < value.size(); ++i) {
                parsed <<= 1;
                if (value[i] == '1') {
                    parsed |= 1ULL;
                }
            }
            return parsed;
        }

        uint64_t parseYosysUnsignedParam(std::string_view value, uint64_t defaultValue = 0) {
            if (value.empty()) {
                return defaultValue;
            }

            bool binaryOnly = true;
            for (char ch : value) {
                if (ch != '0' && ch != '1') {
                    binaryOnly = false;
                    break;
                }
            }

            if (binaryOnly) {
                return parseYosysBinaryUnsigned(value);
            }

            std::string normalized(value);
            if (!normalized.empty() && normalized.front() == '\\') {
                return defaultValue;
            }

            try {
                return static_cast<uint64_t>(std::stoull(normalized, nullptr, 0));
            } catch (const std::exception &) {
                return defaultValue;
            }
        }

        bool parseYosysBoolParam(std::string_view value, bool defaultValue = false) {
            if (value.empty()) {
                return defaultValue;
            }

            if (value == "true") {
                return true;
            }
            if (value == "false") {
                return false;
            }

            return parseYosysUnsignedParam(value, defaultValue ? 1 : 0) != 0;
        }

        uint64_t getCellParamUInt(const Cell &cell,
                                  std::string_view key,
                                  uint64_t defaultValue = 0) {
            const auto it = cell.parameters.find(std::string(key));
            if (it == cell.parameters.end()) {
                return defaultValue;
            }
            return parseYosysUnsignedParam(it->second, defaultValue);
        }

        bool getCellParamBool(const Cell &cell,
                              std::string_view key,
                              bool defaultValue = false) {
            const auto it = cell.parameters.find(std::string(key));
            if (it == cell.parameters.end()) {
                return defaultValue;
            }
            return parseYosysBoolParam(it->second, defaultValue);
        }

        std::string getCellParamString(const Cell &cell,
                                       std::string_view key,
                                       const std::string &defaultValue = {}) {
            const auto it = cell.parameters.find(std::string(key));
            if (it == cell.parameters.end()) {
                return defaultValue;
            }
            return it->second;
        }

        using BitVector = std::vector<uint8_t>;

        BitVector readBitVector(const std::vector<SlotState> &inputs,
                                size_t offset,
                                size_t width) {
            BitVector bits(width, 0);
            for (size_t i = 0; i < width; ++i) {
                const size_t index = offset + i;
                if (index < inputs.size() && inputs[index].state == LogicState::high) {
                    bits[i] = 1;
                }
            }
            return bits;
        }

        bool sliceAnyHigh(const std::vector<SlotState> &inputs,
                          size_t offset,
                          size_t width) {
            for (size_t i = 0; i < width; ++i) {
                const size_t index = offset + i;
                if (index < inputs.size() && inputs[index].state == LogicState::high) {
                    return true;
                }
            }
            return false;
        }

        bool sliceAllUnknownOrHighZ(const std::vector<SlotState> &inputs,
                                    size_t offset,
                                    size_t width) {
            if (width == 0) {
                return false;
            }

            for (size_t i = 0; i < width; ++i) {
                const size_t index = offset + i;
                if (index >= inputs.size()) {
                    return false;
                }

                const auto state = inputs[index].state;
                if (state != LogicState::unknown && state != LogicState::high_z) {
                    return false;
                }
            }
            return true;
        }

        bool isReadEnableActive(const std::vector<SlotState> &inputs,
                                size_t offset,
                                size_t width) {
            if (width == 0) {
                return true;
            }
            if (sliceAnyHigh(inputs, offset, width)) {
                return true;
            }
            return sliceAllUnknownOrHighZ(inputs, offset, width);
        }

        bool isWriteEnableActive(const std::vector<SlotState> &inputs,
                                 size_t offset,
                                 size_t width) {
            if (width == 0) {
                return true;
            }
            return sliceAnyHigh(inputs, offset, width);
        }

        uint8_t bitAt(const BitVector &bits,
                      size_t index,
                      bool signExtend = false) {
            if (index < bits.size()) {
                return bits[index] ? 1 : 0;
            }
            if (signExtend && !bits.empty()) {
                return bits.back() ? 1 : 0;
            }
            return 0;
        }

        size_t bitVectorToSize(const BitVector &bits) {
            size_t value = 0;
            const size_t limit = std::min(bits.size(), sizeof(size_t) * 8);
            for (size_t i = 0; i < limit; ++i) {
                if (bits[i]) {
                    value |= (static_cast<size_t>(1) << i);
                }
            }
            return value;
        }

        BitVector addBitVectors(const BitVector &a,
                                bool aSigned,
                                const BitVector &b,
                                bool bSigned,
                                size_t outputWidth) {
            BitVector out(outputWidth, 0);
            uint8_t carry = 0;
            for (size_t i = 0; i < outputWidth; ++i) {
                const uint8_t av = bitAt(a, i, aSigned);
                const uint8_t bv = bitAt(b, i, bSigned);
                const uint8_t sum = static_cast<uint8_t>(av + bv + carry);
                out[i] = sum & 1U;
                carry = (sum >> 1U) & 1U;
            }
            return out;
        }

        BitVector subBitVectors(const BitVector &a,
                                bool aSigned,
                                const BitVector &b,
                                bool bSigned,
                                size_t outputWidth) {
            BitVector out(outputWidth, 0);
            uint8_t borrow = 0;
            for (size_t i = 0; i < outputWidth; ++i) {
                const int av = bitAt(a, i, aSigned);
                const int bv = bitAt(b, i, bSigned);
                int diff = av - bv - static_cast<int>(borrow);
                if (diff < 0) {
                    diff += 2;
                    borrow = 1;
                } else {
                    borrow = 0;
                }
                out[i] = static_cast<uint8_t>(diff);
            }
            return out;
        }

        BitVector mulBitVectors(const BitVector &a,
                                bool aSigned,
                                const BitVector &b,
                                bool bSigned,
                                size_t outputWidth) {
            BitVector out(outputWidth, 0);
            for (size_t i = 0; i < outputWidth; ++i) {
                if (!bitAt(b, i, bSigned)) {
                    continue;
                }

                uint8_t carry = 0;
                for (size_t j = 0; j + i < outputWidth; ++j) {
                    const uint8_t av = bitAt(a, j, aSigned);
                    const uint8_t sum = static_cast<uint8_t>(out[j + i] + av + carry);
                    out[j + i] = sum & 1U;
                    carry = (sum >> 1U) & 1U;
                }
            }
            return out;
        }

        int compareBitVectors(const BitVector &a,
                              bool aSigned,
                              const BitVector &b,
                              bool bSigned) {
            const size_t width = std::max(a.size(), b.size());
            if (width == 0) {
                return 0;
            }

            const uint8_t signA = bitAt(a, width - 1, aSigned);
            const uint8_t signB = bitAt(b, width - 1, bSigned);
            if (signA != signB) {
                return signA ? -1 : 1;
            }

            for (size_t i = width; i > 0; --i) {
                const size_t index = i - 1;
                const uint8_t av = bitAt(a, index, aSigned);
                const uint8_t bv = bitAt(b, index, bSigned);
                if (av == bv) {
                    continue;
                }
                return av > bv ? 1 : -1;
            }
            return 0;
        }

        BitVector shiftLeftBitVector(const BitVector &a,
                                     size_t shift,
                                     size_t outputWidth) {
            BitVector out(outputWidth, 0);
            if (shift >= outputWidth) {
                return out;
            }
            for (size_t i = shift; i < outputWidth; ++i) {
                out[i] = bitAt(a, i - shift, false);
            }
            return out;
        }

        BitVector shiftRightBitVector(const BitVector &a,
                                      size_t shift,
                                      size_t outputWidth,
                                      bool arithmetic) {
            BitVector out(outputWidth, 0);
            const uint8_t fill = (arithmetic && !a.empty()) ? a.back() : 0;
            for (size_t i = 0; i < outputWidth; ++i) {
                const size_t source = i + shift;
                if (source < a.size()) {
                    out[i] = a[source];
                } else {
                    out[i] = fill;
                }
            }
            return out;
        }

        bool applyOutputBits(ComponentState &next,
                             const ComponentState &prevState,
                             const BitVector &outputBits,
                             SimTime simTime) {
            bool changed = false;
            const size_t count = std::min(next.outputStates.size(), outputBits.size());
            for (size_t i = 0; i < count; ++i) {
                const auto newState = outputBits[i] ? LogicState::high : LogicState::low;
                changed = changed || prevState.outputStates[i].state != newState;
                next.outputStates[i] = {newState, simTime};
            }
            next.isChanged = changed;
            return changed;
        }

        std::vector<std::string> makeIndexedSlotNames(const std::string &prefix, size_t count) {
            std::vector<std::string> names;
            names.reserve(count);
            for (size_t i = 0; i < count; ++i) {
                names.push_back(prefix + std::to_string(i));
            }
            return names;
        }

        std::shared_ptr<ComponentDefinition> ensureCustomDefinition(
            const std::string &name,
            const SlotsGroupInfo &inputs,
            const SlotsGroupInfo &outputs,
            const SimulationFunction &simulationFunction) {
            auto definition = findDefinitionByName(name);
            if (definition) {
                return definition;
            }

            auto created = std::make_shared<ComponentDefinition>();
            created->setName(name);
            created->setGroupName("Verilog Imported");
            created->setInputSlotsInfo(inputs);
            created->setOutputSlotsInfo(outputs);
            created->setSimulationFunction(simulationFunction);
            created->setSimDelay(SimDelayNanoSeconds(2));

            ComponentCatalog::instance().registerComponent(created);
            definition = findDefinitionByName(name);
            if (!definition) {
                throw std::runtime_error("Failed to register component definition: " + name);
            }
            return definition;
        }

        std::shared_ptr<ComponentDefinition> ensureArithmeticDefinition(const std::string &cellType,
                                                                        size_t aWidth,
                                                                        size_t bWidth,
                                                                        size_t yWidth,
                                                                        bool aSigned,
                                                                        bool bSigned) {
            const auto name = std::string("Verilog ") + cellType +
                              " A" + std::to_string(aWidth) +
                              (aSigned ? "s" : "u") +
                              " B" + std::to_string(bWidth) +
                              (bSigned ? "s" : "u") +
                              " Y" + std::to_string(yWidth);

            const SlotsGroupInfo inputs{
                SlotsGroupType::input,
                false,
                aWidth + bWidth,
                makeIndexedSlotNames("I", aWidth + bWidth),
                {},
            };

            const SlotsGroupInfo outputs{
                SlotsGroupType::output,
                false,
                yWidth,
                makeIndexedSlotNames("Y", yWidth),
                {},
            };

            auto simFn = [cellType, aWidth, bWidth, yWidth, aSigned, bSigned](
                             const std::vector<SlotState> &inputs,
                             SimTime simTime,
                             const ComponentState &prevState) {
                auto next = prevState;
                next.inputStates = inputs;
                const auto a = readBitVector(inputs, 0, aWidth);
                const auto b = readBitVector(inputs, aWidth, bWidth);

                BitVector result;
                if (cellType == "$add") {
                    result = addBitVectors(a, aSigned, b, bSigned, yWidth);
                } else if (cellType == "$sub") {
                    result = subBitVectors(a, aSigned, b, bSigned, yWidth);
                } else {
                    result = mulBitVectors(a, aSigned, b, bSigned, yWidth);
                }

                applyOutputBits(next, prevState, result, simTime);
                return next;
            };

            return ensureCustomDefinition(name, inputs, outputs, simFn);
        }

        std::shared_ptr<ComponentDefinition> ensureComparatorDefinition(const std::string &cellType,
                                                                        size_t aWidth,
                                                                        size_t bWidth,
                                                                        bool aSigned,
                                                                        bool bSigned,
                                                                        size_t yWidth) {
            const auto name = std::string("Verilog ") + cellType +
                              " A" + std::to_string(aWidth) +
                              (aSigned ? "s" : "u") +
                              " B" + std::to_string(bWidth) +
                              (bSigned ? "s" : "u") +
                              " Y" + std::to_string(yWidth);

            const SlotsGroupInfo inputs{
                SlotsGroupType::input,
                false,
                aWidth + bWidth,
                makeIndexedSlotNames("I", aWidth + bWidth),
                {},
            };

            const SlotsGroupInfo outputs{
                SlotsGroupType::output,
                false,
                yWidth,
                makeIndexedSlotNames("Y", yWidth),
                {},
            };

            auto simFn = [cellType, aWidth, bWidth, yWidth, aSigned, bSigned](
                             const std::vector<SlotState> &inputs,
                             SimTime simTime,
                             const ComponentState &prevState) {
                auto next = prevState;
                next.inputStates = inputs;
                const auto a = readBitVector(inputs, 0, aWidth);
                const auto b = readBitVector(inputs, aWidth, bWidth);
                const int cmp = compareBitVectors(a, aSigned, b, bSigned);

                bool resultBit = false;
                if (cellType == "$eq") {
                    resultBit = cmp == 0;
                } else if (cellType == "$ne") {
                    resultBit = cmp != 0;
                } else if (cellType == "$lt") {
                    resultBit = cmp < 0;
                } else if (cellType == "$le") {
                    resultBit = cmp <= 0;
                } else if (cellType == "$gt") {
                    resultBit = cmp > 0;
                } else if (cellType == "$ge") {
                    resultBit = cmp >= 0;
                }

                BitVector out(yWidth, 0);
                if (!out.empty()) {
                    out[0] = resultBit ? 1 : 0;
                }
                applyOutputBits(next, prevState, out, simTime);
                return next;
            };

            return ensureCustomDefinition(name, inputs, outputs, simFn);
        }

        std::shared_ptr<ComponentDefinition> ensureLogicDefinition(const std::string &cellType,
                                                                   size_t aWidth,
                                                                   size_t bWidth,
                                                                   size_t yWidth) {
            const auto name = std::string("Verilog ") + cellType +
                              " A" + std::to_string(aWidth) +
                              " B" + std::to_string(bWidth) +
                              " Y" + std::to_string(yWidth);

            const size_t inputCount = cellType == "$logic_not" ? aWidth : (aWidth + bWidth);

            const SlotsGroupInfo inputs{
                SlotsGroupType::input,
                false,
                inputCount,
                makeIndexedSlotNames("I", inputCount),
                {},
            };

            const SlotsGroupInfo outputs{
                SlotsGroupType::output,
                false,
                yWidth,
                makeIndexedSlotNames("Y", yWidth),
                {},
            };

            auto simFn = [cellType, aWidth, bWidth, yWidth](
                             const std::vector<SlotState> &inputs,
                             SimTime simTime,
                             const ComponentState &prevState) {
                auto next = prevState;
                next.inputStates = inputs;

                const bool aTrue = sliceAnyHigh(inputs, 0, aWidth);
                bool resultBit = false;
                if (cellType == "$logic_not") {
                    resultBit = !aTrue;
                } else {
                    const bool bTrue = sliceAnyHigh(inputs, aWidth, bWidth);
                    if (cellType == "$logic_and") {
                        resultBit = aTrue && bTrue;
                    } else {
                        resultBit = aTrue || bTrue;
                    }
                }

                BitVector out(yWidth, 0);
                if (!out.empty()) {
                    out[0] = resultBit ? 1 : 0;
                }
                applyOutputBits(next, prevState, out, simTime);
                return next;
            };

            return ensureCustomDefinition(name, inputs, outputs, simFn);
        }

        std::shared_ptr<ComponentDefinition> ensureShiftDefinition(const std::string &cellType,
                                                                   size_t aWidth,
                                                                   size_t bWidth,
                                                                   size_t yWidth,
                                                                   bool aSigned) {
            const auto name = std::string("Verilog ") + cellType +
                              " A" + std::to_string(aWidth) +
                              (aSigned ? "s" : "u") +
                              " B" + std::to_string(bWidth) +
                              " Y" + std::to_string(yWidth);

            const SlotsGroupInfo inputs{
                SlotsGroupType::input,
                false,
                aWidth + bWidth,
                makeIndexedSlotNames("I", aWidth + bWidth),
                {},
            };

            const SlotsGroupInfo outputs{
                SlotsGroupType::output,
                false,
                yWidth,
                makeIndexedSlotNames("Y", yWidth),
                {},
            };

            auto simFn = [cellType, aWidth, bWidth, yWidth, aSigned](
                             const std::vector<SlotState> &inputs,
                             SimTime simTime,
                             const ComponentState &prevState) {
                auto next = prevState;
                next.inputStates = inputs;

                const auto a = readBitVector(inputs, 0, aWidth);
                const auto b = readBitVector(inputs, aWidth, bWidth);
                const size_t shiftBy = bitVectorToSize(b);

                BitVector out;
                if (cellType == "$shl" || cellType == "$sshl" || cellType == "$shiftx") {
                    out = shiftLeftBitVector(a, shiftBy, yWidth);
                } else {
                    const bool arithmetic = (cellType == "$sshr") && aSigned;
                    out = shiftRightBitVector(a, shiftBy, yWidth, arithmetic);
                }

                applyOutputBits(next, prevState, out, simTime);
                return next;
            };

            return ensureCustomDefinition(name, inputs, outputs, simFn);
        }

        std::shared_ptr<ComponentDefinition> ensureLatchDefinition(size_t width,
                                                                   bool enableActiveHigh) {
            const auto name = std::string("Verilog Latch ") +
                              (enableActiveHigh ? "EN+" : "EN-") +
                              " W" + std::to_string(width);

            auto inputNames = makeIndexedSlotNames("D", width);
            inputNames.push_back("EN");

            SlotsGroupInfo inputs{
                SlotsGroupType::input,
                false,
                width + 1,
                inputNames,
                {},
            };
            inputs.categories.push_back({static_cast<int>(width), SlotCatergory::enable});

            const SlotsGroupInfo outputs{
                SlotsGroupType::output,
                false,
                width,
                makeIndexedSlotNames("Q", width),
                {},
            };

            auto simFn = [width, enableActiveHigh](const std::vector<SlotState> &inputs,
                                                    SimTime simTime,
                                                    const ComponentState &prevState) {
                auto next = prevState;
                next.inputStates = inputs;
                next.isChanged = false;

                const auto enState = inputs[width].state == LogicState::high;
                const bool enabled = enableActiveHigh ? enState : !enState;
                if (!enabled) {
                    return next;
                }

                BitVector out(width, 0);
                for (size_t i = 0; i < width; ++i) {
                    out[i] = inputs[i].state == LogicState::high ? 1 : 0;
                }

                applyOutputBits(next, prevState, out, simTime);
                return next;
            };

            return ensureCustomDefinition(name, inputs, outputs, simFn);
        }

        std::shared_ptr<ComponentDefinition> ensurePmuxDefinition(size_t width,
                                                                  size_t selectWidth) {
            const auto name = std::string("Verilog $pmux W") + std::to_string(width) +
                              " S" + std::to_string(selectWidth);

            const size_t inputCount = width + (width * selectWidth) + selectWidth;
            const SlotsGroupInfo inputs{
                SlotsGroupType::input,
                false,
                inputCount,
                makeIndexedSlotNames("I", inputCount),
                {},
            };

            const SlotsGroupInfo outputs{
                SlotsGroupType::output,
                false,
                width,
                makeIndexedSlotNames("Y", width),
                {},
            };

            auto simFn = [width, selectWidth](const std::vector<SlotState> &inputs,
                                              SimTime simTime,
                                              const ComponentState &prevState) {
                auto next = prevState;
                next.inputStates = inputs;

                const size_t aOffset = 0;
                const size_t bOffset = width;
                const size_t sOffset = width + (width * selectWidth);

                BitVector out = readBitVector(inputs, aOffset, width);
                for (size_t i = 0; i < selectWidth; ++i) {
                    const bool selected = (sOffset + i) < inputs.size() &&
                                          inputs[sOffset + i].state == LogicState::high;
                    if (!selected) {
                        continue;
                    }

                    out = readBitVector(inputs, bOffset + (i * width), width);
                    break;
                }

                applyOutputBits(next, prevState, out, simTime);
                return next;
            };

            return ensureCustomDefinition(name, inputs, outputs, simFn);
        }

        struct ImportedMemoryCore {
            size_t width = 0;
            std::unordered_map<size_t, BitVector> words;
        };

        bool detectClockEdge(const std::vector<SlotState> &inputs,
                             const ComponentState &prevState,
                             size_t clkSlot,
                             bool risingEdge) {
            const bool currentClock = clkSlot < inputs.size() &&
                                      inputs[clkSlot].state == LogicState::high;
            const bool previousClock = clkSlot < prevState.inputStates.size() &&
                                       prevState.inputStates[clkSlot].state == LogicState::high;
            if (risingEdge) {
                return !previousClock && currentClock;
            }
            return previousClock && !currentClock;
        }

        std::shared_ptr<ComponentDefinition> ensureMemoryReadDefinition(
            const std::string &memoryKey,
            const std::shared_ptr<ImportedMemoryCore> &memory,
            size_t addrWidth,
            size_t enWidth,
            size_t dataWidth,
            bool hasClockInput,
            bool clockEnabled,
            bool risingEdge,
            bool hasSyncInput) {
            const auto name = std::string("Verilog MemRead ") + memoryKey +
                              " A" + std::to_string(addrWidth) +
                              " D" + std::to_string(dataWidth);

            const size_t inputCount = addrWidth + enWidth + (hasClockInput ? 1 : 0) + (hasSyncInput ? 1 : 0);

            const SlotsGroupInfo inputs{
                SlotsGroupType::input,
                false,
                inputCount,
                makeIndexedSlotNames("I", inputCount),
                {},
            };

            const SlotsGroupInfo outputs{
                SlotsGroupType::output,
                false,
                dataWidth,
                makeIndexedSlotNames("Y", dataWidth),
                {},
            };

            auto simFn = [memory, addrWidth, enWidth, dataWidth, hasClockInput, clockEnabled, risingEdge](
                             const std::vector<SlotState> &inputs,
                             SimTime simTime,
                             const ComponentState &prevState) {
                auto next = prevState;
                next.inputStates = inputs;
                next.isChanged = false;

                const size_t clkSlot = addrWidth + enWidth;

                if (!isReadEnableActive(inputs, addrWidth, enWidth)) {
                    return next;
                }

                if (clockEnabled && hasClockInput && !detectClockEdge(inputs, prevState, clkSlot, risingEdge)) {
                    return next;
                }

                const auto addrBits = readBitVector(inputs, 0, addrWidth);
                const size_t address = bitVectorToSize(addrBits);

                BitVector out(dataWidth, 0);
                if (const auto it = memory->words.find(address); it != memory->words.end()) {
                    const auto &stored = it->second;
                    for (size_t i = 0; i < dataWidth; ++i) {
                        out[i] = bitAt(stored, i, false);
                    }
                }

                applyOutputBits(next, prevState, out, simTime);
                return next;
            };

            return ensureCustomDefinition(name, inputs, outputs, simFn);
        }

        std::shared_ptr<ComponentDefinition> ensureMemoryWriteDefinition(
            const std::string &memoryKey,
            const std::shared_ptr<ImportedMemoryCore> &memory,
            size_t addrWidth,
            size_t dataWidth,
            size_t enWidth,
            bool hasClockInput,
            bool clockEnabled,
            bool risingEdge) {
            const auto name = std::string("Verilog MemWrite ") + memoryKey +
                              " A" + std::to_string(addrWidth) +
                              " D" + std::to_string(dataWidth);

            const size_t inputCount = addrWidth + dataWidth + enWidth + (hasClockInput ? 1 : 0);

            const SlotsGroupInfo inputs{
                SlotsGroupType::input,
                false,
                inputCount,
                makeIndexedSlotNames("I", inputCount),
                {},
            };

            const SlotsGroupInfo outputs{
                SlotsGroupType::output,
                false,
                1,
                {"sync"},
                {},
            };

            auto simFn = [memory, addrWidth, dataWidth, enWidth, hasClockInput, clockEnabled, risingEdge](
                             const std::vector<SlotState> &inputs,
                             SimTime simTime,
                             const ComponentState &prevState) {
                auto next = prevState;
                next.inputStates = inputs;
                next.isChanged = false;

                const size_t dataOffset = addrWidth;
                const size_t enOffset = dataOffset + dataWidth;
                const size_t clkSlot = enOffset + enWidth;

                if (!isWriteEnableActive(inputs, enOffset, enWidth)) {
                    return next;
                }

                if (clockEnabled && hasClockInput && !detectClockEdge(inputs, prevState, clkSlot, risingEdge)) {
                    return next;
                }

                const auto addrBits = readBitVector(inputs, 0, addrWidth);
                const size_t address = bitVectorToSize(addrBits);
                auto &word = memory->words[address];
                if (word.size() < dataWidth) {
                    word.resize(dataWidth, 0);
                }

                const auto dataBits = readBitVector(inputs, dataOffset, dataWidth);
                for (size_t i = 0; i < dataWidth; ++i) {
                    bool writeBit = true;
                    if (enWidth == 1) {
                        writeBit = (enOffset < inputs.size() && inputs[enOffset].state == LogicState::high);
                    } else if (enWidth > 1) {
                        const size_t enBitIndex = enOffset + (i % enWidth);
                        writeBit = (enBitIndex < inputs.size() && inputs[enBitIndex].state == LogicState::high);
                    }

                    if (writeBit) {
                        word[i] = bitAt(dataBits, i, false);
                    }
                }

                const bool previousTick = !prevState.outputStates.empty() &&
                                          prevState.outputStates[0].state == LogicState::high;
                const auto newTick = previousTick ? LogicState::low : LogicState::high;
                next.outputStates[0] = {newTick, simTime};
                next.isChanged = true;
                return next;
            };

            return ensureCustomDefinition(name, inputs, outputs, simFn);
        }

        class Importer {
          public:
            Importer(const Design &design, SimulationEngine &engine)
                : m_design(design), m_engine(engine) {}

            SimEngineImportResult importTop(const std::string &topModuleName) {
                const auto &topModule = *requireModule(topModuleName);

                m_result.topModuleName = topModuleName;
                m_result.top.definitionName = topModule.name;
                m_result.top.instancePath = topModuleName;
                m_result.top.parentInstancePath = {};
                m_result.top.inputSlotNames = buildPortSlotNames(topModule, PortDirection::input);
                m_result.top.outputSlotNames = buildPortSlotNames(topModule, PortDirection::output);
                m_result.top.internalInputSinks.resize(m_result.top.inputSlotNames.size());
                m_result.top.internalOutputDrivers.resize(m_result.top.outputSlotNames.size());
                m_result.instancesByPath[topModuleName] = m_result.top;

                PortBindings topBindings;
                initializeTopBoundary(topModule, topBindings);
                elaborateModule(topModule, topModuleName, topBindings);
                materializeConnections();

                // Copy back boundary info populated during elaboration
                m_result.top = m_result.instancesByPath[topModuleName];

                SimEngineImportResult result;
                result = m_result;
                return result;
            }

          private:
            using PortBindings = std::unordered_map<std::string, SignalRef>;

            const Module *requireModule(std::string_view name) const {
                const auto *module = m_design.findModule(name);
                if (!module) {
                    throw std::runtime_error("Module not found in imported design: " + std::string(name));
                }
                return module;
            }

            std::optional<std::string> keyForBit(const SignalBit &bit) const {
                if (!bit.isNet()) {
                    return std::nullopt;
                }
                return std::to_string(*bit.netId);
            }

            std::optional<std::string> scopedNetKey(std::string_view path, const SignalBit &bit) const {
                const auto bitKey = keyForBit(bit);
                if (!bitKey.has_value()) {
                    return std::nullopt;
                }
                return std::string(path) + ":" + *bitKey;
            }

            std::string scopedPortBitKey(std::string_view path, const Port &port, size_t bitIndex) const {
                return std::string(path) + ":port:" + port.name + ":" + std::to_string(bitIndex);
            }

            SignalRef resolveSignal(std::string_view path,
                                    const SignalBit &bit,
                                    const PortBindings &bindings) const {
                if (bit.isConstant()) {
                    return SignalRef::constantValue(*bit.constant);
                }

                const auto signalKey = scopedNetKey(path, bit);
                if (!signalKey.has_value()) {
                    throw std::runtime_error("Unsupported signal bit encoding while resolving imported Verilog signal");
                }

                const auto directPortIt = bindings.find(*signalKey);
                if (directPortIt != bindings.end()) {
                    return directPortIt->second;
                }

                return SignalRef::net(*signalKey);
            }

            void registerDriver(const SignalRef &signal, SlotEndpoint endpoint) {
                if (signal.kind != SignalRefKind::net) {
                    throw std::runtime_error("Only net-backed signals may be used as drivers");
                }
                m_netDrivers[signal.netKey] = endpoint;
            }

            void registerLoad(const SignalRef &signal, SlotEndpoint endpoint) {
                if (signal.kind == SignalRefKind::net) {
                    m_netLoads[signal.netKey].push_back(endpoint);
                    return;
                }
                m_directLoads.emplace_back(signal, endpoint);
            }

            std::shared_ptr<ImportedMemoryCore> getOrCreateMemoryCore(const std::string &memoryKey,
                                                                       size_t width) {
                auto it = m_memories.find(memoryKey);
                if (it != m_memories.end()) {
                    it->second->width = std::max(it->second->width, width);
                    return it->second;
                }

                auto created = std::make_shared<ImportedMemoryCore>();
                created->width = width;
                m_memories.emplace(memoryKey, created);
                return created;
            }

            std::string memorySyncNetKey(const std::string &memoryKey) const {
                return std::string("$memsync:") + memoryKey;
            }

            SlotEndpoint getOrCreateConstantDriver(const std::string &constant) {
                const auto it = m_constantDrivers.find(constant);
                if (it != m_constantDrivers.end()) {
                    return it->second;
                }

                auto inputDefinition = ensureBuiltinIoDefinition("Input");
                const auto id = m_engine.addComponent(inputDefinition);
                auto component = m_engine.getDigitalComponent(id);
                resizeOutputs(component, 1);
                m_engine.setOutputSlotState(id, 0, constantToLogicState(constant));
                m_createdComponentIds.push_back(id);
                m_result.componentInstancePathById[id] = m_result.topModuleName;

                SlotEndpoint endpoint{id, SlotType::digitalOutput, 0};
                m_constantDrivers[constant] = endpoint;
                return endpoint;
            }

            UUID createTopBoundaryComponent(const std::shared_ptr<ComponentDefinition> &definition,
                                            size_t slotCount,
                                            const std::vector<std::string> &slotNames,
                                            bool isInputComponent) {
                const auto id = m_engine.addComponent(definition);
                m_createdComponentIds.push_back(id);
                m_result.componentInstancePathById[id] = m_result.topModuleName;
                auto component = m_engine.getDigitalComponent(id);
                if (isInputComponent) {
                    resizeOutputs(component, std::max<size_t>(1, slotCount));
                    component->definition->getOutputSlotsInfo().names = slotNames;
                } else {
                    resizeInputs(component, std::max<size_t>(1, slotCount));
                    component->definition->getInputSlotsInfo().names = slotNames;
                }
                return id;
            }

            void initializeTopBoundary(const Module &topModule, PortBindings &bindings) {
                auto inputDefinition = ensureBuiltinIoDefinition("Input");
                auto outputDefinition = ensureBuiltinIoDefinition("Output");

                for (const auto &port : topModule.ports) {
                    std::vector<std::string> slotNames;
                    slotNames.reserve(port.bits.size());
                    if (port.bits.size() == 1) {
                        slotNames.push_back(port.name);
                    } else {
                        for (size_t i = 0; i < port.bits.size(); ++i) {
                            slotNames.push_back(port.name + "[" + std::to_string(i) + "]");
                        }
                    }

                    if (port.direction == PortDirection::input) {
                        const auto id = createTopBoundaryComponent(inputDefinition,
                                                                   port.bits.size(),
                                                                   slotNames,
                                                                   true);
                        m_result.topInputComponents[port.name] = id;
                        for (size_t i = 0; i < port.bits.size(); ++i) {
                            const auto signalKey = scopedNetKey(topModule.name, port.bits[i]);
                            if (!signalKey.has_value()) {
                                BESS_WARN("[Verilog Import] Top input port '{}' bit {} resolved to constant and cannot drive internal net",
                                          port.name,
                                          i);
                                continue;
                            }
                            const auto signal = SignalRef::net(*signalKey);
                            registerDriver(signal, SlotEndpoint{id, SlotType::digitalOutput, static_cast<int>(i)});
                        }
                    } else if (port.direction == PortDirection::output) {
                        const auto id = createTopBoundaryComponent(outputDefinition,
                                                                   port.bits.size(),
                                                                   slotNames,
                                                                   false);
                        m_result.topOutputComponents[port.name] = id;
                        for (size_t i = 0; i < port.bits.size(); ++i) {
                            const auto signalKey = scopedNetKey(topModule.name, port.bits[i]);
                            if (signalKey.has_value()) {
                                registerLoad(SignalRef::net(*signalKey),
                                             SlotEndpoint{id, SlotType::digitalInput, static_cast<int>(i)});
                            } else if (port.bits[i].isConstant()) {
                                registerLoad(SignalRef::constantValue(*port.bits[i].constant),
                                             SlotEndpoint{id, SlotType::digitalInput, static_cast<int>(i)});
                            }
                        }
                    } else {
                        BESS_WARN("[Verilog Import] Top-level inout port '{}' is imported as bidirectional boundary with limited multi-driver support",
                                  port.name);

                        const auto inputId = createTopBoundaryComponent(inputDefinition,
                                                                        port.bits.size(),
                                                                        slotNames,
                                                                        true);
                        m_result.topInputComponents[port.name] = inputId;

                        const auto outputId = createTopBoundaryComponent(outputDefinition,
                                                                         port.bits.size(),
                                                                         slotNames,
                                                                         false);
                        m_result.topOutputComponents[port.name] = outputId;

                        for (size_t i = 0; i < port.bits.size(); ++i) {
                            const auto signalKey = scopedNetKey(topModule.name, port.bits[i]);
                            if (signalKey.has_value()) {
                                registerLoad(SignalRef::net(*signalKey),
                                             SlotEndpoint{outputId, SlotType::digitalInput, static_cast<int>(i)});
                                m_pendingTopInputDrivers.emplace_back(*signalKey,
                                                                      SlotEndpoint{inputId,
                                                                                   SlotType::digitalOutput,
                                                                                   static_cast<int>(i)});
                            } else if (port.bits[i].isConstant()) {
                                registerLoad(SignalRef::constantValue(*port.bits[i].constant),
                                             SlotEndpoint{outputId, SlotType::digitalInput, static_cast<int>(i)});
                            }
                        }
                    }
                }
            }

            void elaborateModule(const Module &module,
                                 const std::string &path,
                                 const PortBindings &bindings) {
                std::unordered_map<int64_t, size_t> inputBoundarySlotByNetId;
                std::unordered_map<int64_t, size_t> outputBoundarySlotByNetId;

                if (path != m_result.topModuleName) {
                    ImportedModuleInstance instance;
                    instance.definitionName = module.name;
                    instance.instancePath = path;
                    instance.parentInstancePath = parentInstancePath(path);
                    instance.inputSlotNames = buildPortSlotNames(module, PortDirection::input);
                    instance.outputSlotNames = buildPortSlotNames(module, PortDirection::output);
                    instance.internalInputSinks.resize(instance.inputSlotNames.size());
                    instance.internalOutputDrivers.resize(instance.outputSlotNames.size());
                    m_result.instancesByPath[path] = instance;
                }

                size_t inputSlotIndex = 0;
                for (const auto *port : orderedPortsForDirection(module, PortDirection::input)) {
                    for (const auto &bit : port->bits) {
                        if (bit.isNet()) {
                            inputBoundarySlotByNetId[*bit.netId] = inputSlotIndex;
                        }
                        ++inputSlotIndex;
                    }
                }

                size_t outputSlotIndex = 0;
                for (const auto *port : orderedPortsForDirection(module, PortDirection::output)) {
                    for (const auto &bit : port->bits) {
                        if (bit.isNet()) {
                            outputBoundarySlotByNetId[*bit.netId] = outputSlotIndex;
                        }
                        ++outputSlotIndex;
                    }
                }

                for (const auto &cell : module.cells) {
                    if (const auto *childModule = m_design.findModule(cell.type)) {
                        PortBindings childBindings;
                        for (const auto &childPort : childModule->ports) {
                            const auto connIt = cell.connections.find(childPort.name);
                            if (connIt == cell.connections.end()) {
                                continue;
                            }
                            if (connIt->second.size() != childPort.bits.size()) {
                                throw std::runtime_error("Port width mismatch while importing child module " + path + "/" + cell.name);
                            }

                            for (size_t i = 0; i < childPort.bits.size(); ++i) {
                                const auto bindingKey = scopedNetKey(path + "/" + cell.name, childPort.bits[i]);
                                if (!bindingKey.has_value()) {
                                    continue;
                                }
                                childBindings[*bindingKey] = resolveSignal(path, connIt->second[i], bindings);
                            }
                        }

                        elaborateModule(*childModule, path + "/" + cell.name, childBindings);
                        continue;
                    }

                    instantiatePrimitive(path, cell, bindings, inputBoundarySlotByNetId, outputBoundarySlotByNetId);
                }
            }

            void instantiatePrimitive(const std::string &path,
                                      const Cell &cell,
                                      const PortBindings &bindings,
                                      const std::unordered_map<int64_t, size_t> &inputBoundarySlotByNetId,
                                      const std::unordered_map<int64_t, size_t> &outputBoundarySlotByNetId) {
                auto recordBoundaryInputSink = [&](const SignalBit &bit, const SlotEndpoint &endpoint) {
                    if (!bit.isNet()) {
                        return;
                    }
                    const auto it = inputBoundarySlotByNetId.find(*bit.netId);
                    if (it == inputBoundarySlotByNetId.end()) {
                        return;
                    }
                    m_result.instancesByPath.at(path).internalInputSinks[it->second].push_back(toImportedSlotEndpoint(endpoint));
                };

                auto recordBoundaryOutputDriver = [&](const SignalBit &bit, const SlotEndpoint &endpoint) {
                    if (!bit.isNet()) {
                        return;
                    }
                    const auto it = outputBoundarySlotByNetId.find(*bit.netId);
                    if (it == outputBoundarySlotByNetId.end()) {
                        return;
                    }
                    m_result.instancesByPath.at(path).internalOutputDrivers[it->second].push_back(toImportedSlotEndpoint(endpoint));
                };

                auto instantiateVectorPrimitive = [&](const std::shared_ptr<ComponentDefinition> &definition,
                                                      const std::vector<SignalBit> &inputBits,
                                                      const std::vector<SignalBit> &outputBits) {
                    const auto componentId = m_engine.addComponent(definition);
                    m_createdComponentIds.push_back(componentId);
                    m_result.componentInstancePathById[componentId] = path;

                    for (size_t i = 0; i < inputBits.size(); ++i) {
                        const SlotEndpoint endpoint{componentId, SlotType::digitalInput, static_cast<int>(i)};
                        registerLoad(resolveSignal(path, inputBits[i], bindings), endpoint);
                        recordBoundaryInputSink(inputBits[i], endpoint);
                    }

                    for (size_t i = 0; i < outputBits.size(); ++i) {
                        const SlotEndpoint endpoint{componentId, SlotType::digitalOutput, static_cast<int>(i)};
                        registerDriver(resolveSignal(path, outputBits[i], bindings), endpoint);
                        recordBoundaryOutputDriver(outputBits[i], endpoint);
                    }
                };

                if (cell.type == "$add" || cell.type == "$sub" || cell.type == "$mul") {
                    const auto &aBits = cell.connections.at("A");
                    const auto &bBits = cell.connections.at("B");
                    const auto &yBits = cell.connections.at("Y");

                    auto definition = ensureArithmeticDefinition(
                        cell.type,
                        aBits.size(),
                        bBits.size(),
                        yBits.size(),
                        getCellParamBool(cell, "A_SIGNED", false),
                        getCellParamBool(cell, "B_SIGNED", false));

                    std::vector<SignalBit> inputs;
                    inputs.reserve(aBits.size() + bBits.size());
                    inputs.insert(inputs.end(), aBits.begin(), aBits.end());
                    inputs.insert(inputs.end(), bBits.begin(), bBits.end());
                    instantiateVectorPrimitive(definition, inputs, yBits);
                    return;
                }

                if (cell.type == "$eq" || cell.type == "$ne" ||
                    cell.type == "$lt" || cell.type == "$le" ||
                    cell.type == "$gt" || cell.type == "$ge") {
                    const auto &aBits = cell.connections.at("A");
                    const auto &bBits = cell.connections.at("B");
                    const auto &yBits = cell.connections.at("Y");

                    auto definition = ensureComparatorDefinition(
                        cell.type,
                        aBits.size(),
                        bBits.size(),
                        getCellParamBool(cell, "A_SIGNED", false),
                        getCellParamBool(cell, "B_SIGNED", false),
                        yBits.size());

                    std::vector<SignalBit> inputs;
                    inputs.reserve(aBits.size() + bBits.size());
                    inputs.insert(inputs.end(), aBits.begin(), aBits.end());
                    inputs.insert(inputs.end(), bBits.begin(), bBits.end());
                    instantiateVectorPrimitive(definition, inputs, yBits);
                    return;
                }

                if (cell.type == "$shl" || cell.type == "$shr" ||
                    cell.type == "$sshl" || cell.type == "$sshr" ||
                    cell.type == "$shiftx") {
                    const auto &aBits = cell.connections.at("A");
                    const auto &bBits = cell.connections.at("B");
                    const auto &yBits = cell.connections.at("Y");

                    auto definition = ensureShiftDefinition(
                        cell.type,
                        aBits.size(),
                        bBits.size(),
                        yBits.size(),
                        getCellParamBool(cell, "A_SIGNED", false));

                    std::vector<SignalBit> inputs;
                    inputs.reserve(aBits.size() + bBits.size());
                    inputs.insert(inputs.end(), aBits.begin(), aBits.end());
                    inputs.insert(inputs.end(), bBits.begin(), bBits.end());
                    instantiateVectorPrimitive(definition, inputs, yBits);
                    return;
                }

                if (cell.type == "$logic_not" || cell.type == "$logic_and" || cell.type == "$logic_or") {
                    const auto &aBits = cell.connections.at("A");
                    const auto &yBits = cell.connections.at("Y");
                    const std::vector<SignalBit> emptyB;
                    const auto &bBits = cell.connections.contains("B") ? cell.connections.at("B") : emptyB;

                    auto definition = ensureLogicDefinition(cell.type,
                                                           aBits.size(),
                                                           bBits.size(),
                                                           yBits.size());

                    std::vector<SignalBit> inputs;
                    inputs.reserve(aBits.size() + bBits.size());
                    inputs.insert(inputs.end(), aBits.begin(), aBits.end());
                    inputs.insert(inputs.end(), bBits.begin(), bBits.end());
                    instantiateVectorPrimitive(definition, inputs, yBits);
                    return;
                }

                if (cell.type == "$pmux") {
                    const auto &aBits = cell.connections.at("A");
                    const auto &bBits = cell.connections.at("B");
                    const auto &sBits = cell.connections.at("S");
                    const auto &yBits = cell.connections.at("Y");

                    if (yBits.size() != aBits.size() ||
                        bBits.size() != yBits.size() * sBits.size()) {
                        throw std::runtime_error("Unsupported pmux width configuration in " + cell.name);
                    }

                    auto definition = ensurePmuxDefinition(yBits.size(), sBits.size());

                    std::vector<SignalBit> inputs;
                    inputs.reserve(aBits.size() + bBits.size() + sBits.size());
                    inputs.insert(inputs.end(), aBits.begin(), aBits.end());
                    inputs.insert(inputs.end(), bBits.begin(), bBits.end());
                    inputs.insert(inputs.end(), sBits.begin(), sBits.end());
                    instantiateVectorPrimitive(definition, inputs, yBits);
                    return;
                }

                if (cell.type == "$dlatch" || cell.type == "$_DLATCH_P_" || cell.type == "$_DLATCH_N_") {
                    const auto &dBits = cell.connections.at("D");
                    const auto &qBits = cell.connections.at("Q");
                    const auto &enBits = cell.connections.at(cell.connections.contains("EN") ? "EN" : "E");

                    if (enBits.size() != 1 || dBits.size() != qBits.size()) {
                        throw std::runtime_error("Unsupported latch width configuration in " + cell.name);
                    }

                    bool enableActiveHigh = true;
                    if (cell.type == "$_DLATCH_N_") {
                        enableActiveHigh = false;
                    } else if (cell.type == "$dlatch") {
                        enableActiveHigh = getCellParamBool(cell, "EN_POLARITY", true);
                    }

                    auto definition = ensureLatchDefinition(dBits.size(), enableActiveHigh);

                    std::vector<SignalBit> inputs;
                    inputs.reserve(dBits.size() + 1);
                    inputs.insert(inputs.end(), dBits.begin(), dBits.end());
                    inputs.push_back(enBits[0]);
                    instantiateVectorPrimitive(definition, inputs, qBits);
                    return;
                }

                if (cell.type == "$memrd" || cell.type == "$memrd_v2") {
                    const auto &addrBits = cell.connections.at("ADDR");
                    const auto &dataBits = cell.connections.at("DATA");
                    const std::vector<SignalBit> emptyBits;
                    const auto &enBits = cell.connections.contains("EN") ? cell.connections.at("EN") : emptyBits;
                    const auto &clkBits = cell.connections.contains("CLK") ? cell.connections.at("CLK") : emptyBits;

                    if (!clkBits.empty() && clkBits.size() != 1) {
                        throw std::runtime_error("Unsupported memory read clock width in " + cell.name);
                    }

                    const auto memId = getCellParamString(cell, "MEMID", cell.name);
                    const auto memoryKey = path + ":" + memId;
                    auto memory = getOrCreateMemoryCore(memoryKey, dataBits.size());

                    const bool hasClockInput = !clkBits.empty();
                    const bool clockEnabled = getCellParamBool(cell, "CLK_ENABLE", hasClockInput);
                    const bool risingEdge = getCellParamBool(cell, "CLK_POLARITY", true);
                    const bool hasSyncInput = true;

                    auto definition = ensureMemoryReadDefinition(memoryKey,
                                                                 memory,
                                                                 addrBits.size(),
                                                                 enBits.size(),
                                                                 dataBits.size(),
                                                                 hasClockInput,
                                                                 clockEnabled,
                                                                 risingEdge,
                                                                 hasSyncInput);

                    const auto componentId = m_engine.addComponent(definition);
                    m_createdComponentIds.push_back(componentId);
                    m_result.componentInstancePathById[componentId] = path;

                    int inputIndex = 0;
                    for (const auto &bit : addrBits) {
                        const SlotEndpoint endpoint{componentId, SlotType::digitalInput, inputIndex++};
                        registerLoad(resolveSignal(path, bit, bindings), endpoint);
                        recordBoundaryInputSink(bit, endpoint);
                    }

                    for (const auto &bit : enBits) {
                        const SlotEndpoint endpoint{componentId, SlotType::digitalInput, inputIndex++};
                        registerLoad(resolveSignal(path, bit, bindings), endpoint);
                        recordBoundaryInputSink(bit, endpoint);
                    }

                    if (hasClockInput) {
                        const SlotEndpoint endpoint{componentId, SlotType::digitalInput, inputIndex++};
                        registerLoad(resolveSignal(path, clkBits[0], bindings), endpoint);
                        recordBoundaryInputSink(clkBits[0], endpoint);
                    }

                    const auto syncNet = memorySyncNetKey(memoryKey);
                    registerLoad(SignalRef::net(syncNet),
                                 SlotEndpoint{componentId, SlotType::digitalInput, inputIndex});

                    for (size_t i = 0; i < dataBits.size(); ++i) {
                        const SlotEndpoint endpoint{componentId, SlotType::digitalOutput, static_cast<int>(i)};
                        registerDriver(resolveSignal(path, dataBits[i], bindings), endpoint);
                        recordBoundaryOutputDriver(dataBits[i], endpoint);
                    }
                    return;
                }

                if (cell.type == "$memwr" || cell.type == "$memwr_v2") {
                    const auto &addrBits = cell.connections.at("ADDR");
                    const auto &dataBits = cell.connections.at("DATA");
                    const std::vector<SignalBit> emptyBits;
                    const auto &enBits = cell.connections.contains("EN") ? cell.connections.at("EN") : emptyBits;
                    const auto &clkBits = cell.connections.contains("CLK") ? cell.connections.at("CLK") : emptyBits;

                    if (!clkBits.empty() && clkBits.size() != 1) {
                        throw std::runtime_error("Unsupported memory write clock width in " + cell.name);
                    }

                    const auto memId = getCellParamString(cell, "MEMID", cell.name);
                    const auto memoryKey = path + ":" + memId;
                    auto memory = getOrCreateMemoryCore(memoryKey, dataBits.size());

                    const bool hasClockInput = !clkBits.empty();
                    const bool clockEnabled = getCellParamBool(cell, "CLK_ENABLE", hasClockInput);
                    const bool risingEdge = getCellParamBool(cell, "CLK_POLARITY", true);

                    auto definition = ensureMemoryWriteDefinition(memoryKey,
                                                                  memory,
                                                                  addrBits.size(),
                                                                  dataBits.size(),
                                                                  enBits.size(),
                                                                  hasClockInput,
                                                                  clockEnabled,
                                                                  risingEdge);

                    const auto componentId = m_engine.addComponent(definition);
                    m_createdComponentIds.push_back(componentId);
                    m_result.componentInstancePathById[componentId] = path;

                    int inputIndex = 0;
                    for (const auto &bit : addrBits) {
                        const SlotEndpoint endpoint{componentId, SlotType::digitalInput, inputIndex++};
                        registerLoad(resolveSignal(path, bit, bindings), endpoint);
                        recordBoundaryInputSink(bit, endpoint);
                    }

                    for (const auto &bit : dataBits) {
                        const SlotEndpoint endpoint{componentId, SlotType::digitalInput, inputIndex++};
                        registerLoad(resolveSignal(path, bit, bindings), endpoint);
                        recordBoundaryInputSink(bit, endpoint);
                    }

                    for (const auto &bit : enBits) {
                        const SlotEndpoint endpoint{componentId, SlotType::digitalInput, inputIndex++};
                        registerLoad(resolveSignal(path, bit, bindings), endpoint);
                        recordBoundaryInputSink(bit, endpoint);
                    }

                    if (hasClockInput) {
                        const SlotEndpoint endpoint{componentId, SlotType::digitalInput, inputIndex++};
                        registerLoad(resolveSignal(path, clkBits[0], bindings), endpoint);
                        recordBoundaryInputSink(clkBits[0], endpoint);
                    }

                    const auto syncNet = memorySyncNetKey(memoryKey);
                    registerDriver(SignalRef::net(syncNet),
                                   SlotEndpoint{componentId, SlotType::digitalOutput, 0});
                    return;
                }

                auto primitiveDefinition = resolvePrimitiveDefinition(cell.type);
                if (!primitiveDefinition) {
                    throw std::runtime_error("Unsupported Yosys cell type during import: " + cell.type);
                }

                if (cell.type == "$_BUF_" || cell.type == "$buf" ||
                    cell.type == "$_NOT_" || cell.type == "$not" || cell.type == "$logic_not") {
                    const auto &inBits = cell.connections.at("A");
                    const auto &outBits = cell.connections.at("Y");
                    if (inBits.size() != outBits.size()) {
                        throw std::runtime_error("Width mismatch in unary primitive " + cell.name);
                    }
                    for (size_t i = 0; i < outBits.size(); ++i) {
                        const auto componentId = m_engine.addComponent(primitiveDefinition);
                        m_createdComponentIds.push_back(componentId);
                        m_result.componentInstancePathById[componentId] = path;
                        const SlotEndpoint inputEndpoint{componentId, SlotType::digitalInput, 0};
                        const SlotEndpoint outputEndpoint{componentId, SlotType::digitalOutput, 0};
                        registerLoad(resolveSignal(path, inBits[i], bindings), inputEndpoint);
                        registerDriver(resolveSignal(path, outBits[i], bindings), outputEndpoint);
                        recordBoundaryInputSink(inBits[i], inputEndpoint);
                        recordBoundaryOutputDriver(outBits[i], outputEndpoint);
                    }
                    return;
                }

                if (cell.type == "$_AND_" || cell.type == "$and" ||
                    cell.type == "$_NAND_" || cell.type == "$nand" ||
                    cell.type == "$_OR_" || cell.type == "$or" ||
                    cell.type == "$_NOR_" || cell.type == "$nor" ||
                    cell.type == "$_XOR_" || cell.type == "$xor" ||
                    cell.type == "$_XNOR_" || cell.type == "$xnor") {
                    const auto &aBits = cell.connections.at("A");
                    const auto &bBits = cell.connections.at("B");
                    const auto &yBits = cell.connections.at("Y");
                    if (aBits.size() != bBits.size() || aBits.size() != yBits.size()) {
                        throw std::runtime_error("Width mismatch in binary primitive " + cell.name);
                    }
                    for (size_t i = 0; i < yBits.size(); ++i) {
                        const auto componentId = m_engine.addComponent(primitiveDefinition);
                        m_createdComponentIds.push_back(componentId);
                        m_result.componentInstancePathById[componentId] = path;
                        const SlotEndpoint aEndpoint{componentId, SlotType::digitalInput, 0};
                        const SlotEndpoint bEndpoint{componentId, SlotType::digitalInput, 1};
                        const SlotEndpoint yEndpoint{componentId, SlotType::digitalOutput, 0};
                        registerLoad(resolveSignal(path, aBits[i], bindings), aEndpoint);
                        registerLoad(resolveSignal(path, bBits[i], bindings), bEndpoint);
                        registerDriver(resolveSignal(path, yBits[i], bindings), yEndpoint);
                        recordBoundaryInputSink(aBits[i], aEndpoint);
                        recordBoundaryInputSink(bBits[i], bEndpoint);
                        recordBoundaryOutputDriver(yBits[i], yEndpoint);
                    }
                    return;
                }

                if (cell.type == "$reduce_and" || cell.type == "$reduce_or" ||
                    cell.type == "$reduce_bool" || cell.type == "$reduce_xor" ||
                    cell.type == "$reduce_xnor") {
                    const auto &aBits = cell.connections.at("A");
                    const auto &yBits = cell.connections.at("Y");
                    if (yBits.size() != 1) {
                        throw std::runtime_error("Reduction primitive must have a single output: " + cell.name);
                    }

                    const auto componentId = m_engine.addComponent(primitiveDefinition);
                    m_createdComponentIds.push_back(componentId);
                    m_result.componentInstancePathById[componentId] = path;
                    auto component = m_engine.getDigitalComponent(componentId);
                    while (component->definition->getInputSlotsInfo().count < aBits.size()) {
                        component->incrementInputCount(true);
                    }
                    for (size_t i = 0; i < aBits.size(); ++i) {
                        const SlotEndpoint inputEndpoint{componentId, SlotType::digitalInput, static_cast<int>(i)};
                        registerLoad(resolveSignal(path, aBits[i], bindings), inputEndpoint);
                        recordBoundaryInputSink(aBits[i], inputEndpoint);
                    }
                    const SlotEndpoint outputEndpoint{componentId, SlotType::digitalOutput, 0};
                    registerDriver(resolveSignal(path, yBits[0], bindings), outputEndpoint);
                    recordBoundaryOutputDriver(yBits[0], outputEndpoint);
                    return;
                }

                if (cell.type == "$_MUX_" || cell.type == "$mux") {
                    const auto &aBits = cell.connections.at("A");
                    const auto &bBits = cell.connections.at("B");
                    const auto &sBits = cell.connections.at("S");
                    const auto &yBits = cell.connections.at("Y");
                    if (sBits.size() != 1 || aBits.size() != bBits.size() || aBits.size() != yBits.size()) {
                        throw std::runtime_error("Unsupported mux width configuration in " + cell.name);
                    }
                    for (size_t i = 0; i < yBits.size(); ++i) {
                        const auto componentId = m_engine.addComponent(primitiveDefinition);
                        m_createdComponentIds.push_back(componentId);
                        m_result.componentInstancePathById[componentId] = path;
                        const SlotEndpoint aEndpoint{componentId, SlotType::digitalInput, 0};
                        const SlotEndpoint bEndpoint{componentId, SlotType::digitalInput, 1};
                        const SlotEndpoint sEndpoint{componentId, SlotType::digitalInput, 2};
                        const SlotEndpoint yEndpoint{componentId, SlotType::digitalOutput, 0};
                        registerLoad(resolveSignal(path, aBits[i], bindings), aEndpoint);
                        registerLoad(resolveSignal(path, bBits[i], bindings), bEndpoint);
                        registerLoad(resolveSignal(path, sBits[0], bindings), sEndpoint);
                        registerDriver(resolveSignal(path, yBits[i], bindings), yEndpoint);
                        recordBoundaryInputSink(aBits[i], aEndpoint);
                        recordBoundaryInputSink(bBits[i], bEndpoint);
                        recordBoundaryInputSink(sBits[0], sEndpoint);
                        recordBoundaryOutputDriver(yBits[i], yEndpoint);
                    }
                    return;
                }

                {
                    const auto dffParams = parseDffCellType(cell.type);
                    if (dffParams.has_value()) {
                        const auto &dp = *dffParams;
                        const auto &dBits = cell.connections.at("D");
                        const auto &qBits = cell.connections.at("Q");
                        const auto *clkBits = [&]() -> const std::vector<SignalBit> * {
                            if (cell.connections.contains("C")) {
                                return &cell.connections.at("C");
                            }
                            if (cell.connections.contains("CLK")) {
                                return &cell.connections.at("CLK");
                            }
                            return nullptr;
                        }();

                        if (!clkBits) {
                            throw std::runtime_error("Missing clock input while importing DFF " + cell.name);
                        }

                        if (clkBits->size() != 1 || dBits.size() != qBits.size()) {
                            throw std::runtime_error("Unsupported DFF width configuration in " + cell.name);
                        }

                        const auto *resetBits = [&]() -> const std::vector<SignalBit> * {
                            if (cell.connections.contains("R")) {
                                return &cell.connections.at("R");
                            }
                            if (cell.connections.contains("ARST")) {
                                return &cell.connections.at("ARST");
                            }
                            if (cell.connections.contains("SRST")) {
                                return &cell.connections.at("SRST");
                            }
                            return nullptr;
                        }();

                        const auto *enableBits = [&]() -> const std::vector<SignalBit> * {
                            if (cell.connections.contains("E")) {
                                return &cell.connections.at("E");
                            }
                            if (cell.connections.contains("EN")) {
                                return &cell.connections.at("EN");
                            }
                            if (cell.connections.contains("CE")) {
                                return &cell.connections.at("CE");
                            }
                            return nullptr;
                        }();

                        for (size_t i = 0; i < qBits.size(); ++i) {
                            const auto componentId = m_engine.addComponent(primitiveDefinition);
                            m_createdComponentIds.push_back(componentId);
                            m_result.componentInstancePathById[componentId] = path;

                            // D → slot 0, CLK → slot 1
                            const SlotEndpoint dEndpoint{componentId, SlotType::digitalInput, 0};
                            const SlotEndpoint clkEndpoint{componentId, SlotType::digitalInput, 1};
                            const SlotEndpoint qEndpoint{componentId, SlotType::digitalOutput, 0};
                            registerLoad(resolveSignal(path, dBits[i], bindings), dEndpoint);
                            registerLoad(resolveSignal(path, (*clkBits)[0], bindings), clkEndpoint);
                            recordBoundaryInputSink(dBits[i], dEndpoint);
                            recordBoundaryInputSink((*clkBits)[0], clkEndpoint);

                            // RST → slot 2 (if definition has reset)
                            if (dp.hasReset) {
                                const SlotEndpoint rstEndpoint{componentId, SlotType::digitalInput,
                                                               dp.rstSlotIndex()};
                                if (resetBits && !resetBits->empty()) {
                                    registerLoad(resolveSignal(path, (*resetBits)[0], bindings), rstEndpoint);
                                    recordBoundaryInputSink((*resetBits)[0], rstEndpoint);
                                } else {
                                    // Tie reset inactive
                                    registerLoad(SignalRef::constantValue(
                                                     dp.resetActiveHigh ? "0" : "1"),
                                                 rstEndpoint);
                                }
                            }

                            // EN → slot 2 or 3 (if definition has enable)
                            if (dp.hasEnable) {
                                const SlotEndpoint enEndpoint{componentId, SlotType::digitalInput,
                                                              dp.enSlotIndex()};
                                if (enableBits && !enableBits->empty()) {
                                    registerLoad(resolveSignal(path, (*enableBits)[0], bindings), enEndpoint);
                                    recordBoundaryInputSink((*enableBits)[0], enEndpoint);
                                } else {
                                    // Tie enable active
                                    registerLoad(SignalRef::constantValue(
                                                     dp.enableActiveHigh ? "1" : "0"),
                                                 enEndpoint);
                                }
                            }

                            registerDriver(resolveSignal(path, qBits[i], bindings), qEndpoint);
                            recordBoundaryOutputDriver(qBits[i], qEndpoint);
                        }
                        return;
                    }
                }

                throw std::runtime_error("Unsupported primitive cell type during import: " + cell.type);
            }

            void materializeConnections() {
                // For inout boundaries, only use the top input component when the net has no internal driver.
                for (const auto &[netKey, endpoint] : m_pendingTopInputDrivers) {
                    if (!m_netDrivers.contains(netKey)) {
                        m_netDrivers[netKey] = endpoint;
                    }
                }

                for (const auto &[signal, sink] : m_directLoads) {
                    SlotEndpoint source;
                    if (signal.kind == SignalRefKind::constant) {
                        source = getOrCreateConstantDriver(signal.constant);
                    } else if (signal.kind == SignalRefKind::endpoint) {
                        source = signal.endpoint;
                    } else {
                        continue;
                    }
                    connectEndpoints(source, sink);
                }

                for (const auto &[netKey, sinks] : m_netLoads) {
                    const auto driverIt = m_netDrivers.find(netKey);
                    if (driverIt == m_netDrivers.end()) {
                        continue;
                    }
                    for (const auto &sink : sinks) {
                        connectEndpoints(driverIt->second, sink);
                    }
                }
                m_result.createdComponentIds = m_createdComponentIds;
            }

            void connectEndpoints(const SlotEndpoint &source, const SlotEndpoint &sink) {
                if (!m_engine.connectComponent(source.componentId,
                                               source.slotIndex,
                                               source.slotType,
                                               sink.componentId,
                                               sink.slotIndex,
                                               sink.slotType)) {
                    throw std::runtime_error("Failed to connect imported Verilog graph");
                }
            }

            const Design &m_design;
            SimulationEngine &m_engine;
            SimEngineImportResult m_result;
            std::unordered_map<std::string, SlotEndpoint> m_netDrivers;
            std::unordered_map<std::string, std::vector<SlotEndpoint>> m_netLoads;
            std::vector<std::pair<SignalRef, SlotEndpoint>> m_directLoads;
            std::unordered_map<std::string, SlotEndpoint> m_constantDrivers;
            std::unordered_map<std::string, std::shared_ptr<ImportedMemoryCore>> m_memories;
            std::vector<std::pair<std::string, SlotEndpoint>> m_pendingTopInputDrivers;
            std::vector<UUID> m_createdComponentIds;
        };
    } // namespace

    SimEngineImportResult importDesignIntoSimulationEngine(const Design &design,
                                                           SimulationEngine &engine,
                                                           const std::optional<std::string> &topModuleName) {
        const auto &resolvedTop = topModuleName.value_or(design.topModuleName);
        if (resolvedTop.empty()) {
            throw std::runtime_error("No top module was provided or detected for Verilog import");
        }

        const auto previousState = engine.getSimulationState();
        engine.setSimulationState(SimulationState::paused);
        Importer importer(design, engine);
        auto result = importer.importTop(resolvedTop);
        engine.setSimulationState(previousState);
        return result;
    }

    SimEngineImportResult importVerilogFileIntoSimulationEngine(const std::filesystem::path &verilogFile,
                                                                SimulationEngine &engine,
                                                                const YosysRunnerConfig &config) {
        return importVerilogFilesIntoSimulationEngine(std::vector<std::filesystem::path>{verilogFile},
                                                      engine,
                                                      config);
    }

    SimEngineImportResult importVerilogFilesIntoSimulationEngine(const std::vector<std::filesystem::path> &verilogFiles,
                                                                 SimulationEngine &engine,
                                                                 const YosysRunnerConfig &config) {
        return importDesignIntoSimulationEngine(importVerilogToDesign(verilogFiles, config),
                                                engine,
                                                config.topModuleName);
    }

    std::shared_ptr<SimEngine::ComponentDefinition> getFromAuxDataJson(Json::Value auxDataJson) {
        BESS_ASSERT(auxDataJson["type"].asString() == VerCompDefAuxData::type,
                    std::format("Expected aux data of type VerCompDefAuxData, got {}",
                                auxDataJson["type"].asString()));

        const auto &id = auxDataJson["id"].asString();
        const auto &data = auxDataJson["data"];

        if (id == "DffParams") {
            DffParams params;
            params.hasReset = data["hasReset"].asBool();
            params.resetActiveHigh = data["resetActiveHigh"].asBool();
            params.hasEnable = data["hasEnable"].asBool();
            params.enableActiveHigh = data["enableActiveHigh"].asBool();
            return ensureGeneralDffDefinition(params);
        }

        return nullptr;
    }
} // namespace Bess::Verilog
