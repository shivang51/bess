#include "bverilog/sim_engine_importer.h"
#include "component_catalog.h"
#include "component_definition.h"
#include "digital_component.h"
#include "expression_evalutator/expr_evaluator.h"
#include "init_components.h"
#include "types.h"
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace Bess::Verilog {
    using namespace Bess::SimEngine;

    namespace {
        struct SlotEndpoint {
            UUID componentId = UUID::null;
            SlotType slotType = SlotType::digitalInput;
            int slotIndex = 0;
        };

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

        std::shared_ptr<ComponentDefinition> ensureDffDefinition(const std::string &name, bool risingEdge) {
            auto definition = findDefinitionByName(name);
            if (definition) {
                return definition;
            }

            auto created = std::make_shared<ComponentDefinition>();
            created->setName(name);
            created->setGroupName("Verilog Imported");
            SlotsGroupInfo inputs{SlotsGroupType::input, false, 3, {"D", "CLK", "CLR"}, {}};
            inputs.categories = {
                {1, SlotCatergory::clock},
                {2, SlotCatergory::clear},
            };
            created->setInputSlotsInfo(inputs);
            created->setOutputSlotsInfo({SlotsGroupType::output, false, 2, {"Q", "Q'"}, {}});
            created->setSimDelay(SimDelayNanoSeconds(2));
            created->setSimulationFunction([risingEdge](const std::vector<SlotState> &inputs,
                                                        SimTime simTime,
                                                        const ComponentState &prevState) -> ComponentState {
                auto next = prevState;
                next.inputStates = inputs;
                next.isChanged = false;

                const auto currentClock = inputs[1].state == LogicState::high;
                const auto previousClock = prevState.inputStates.size() > 1 &&
                                           prevState.inputStates[1].state == LogicState::high;
                const auto clear = inputs[2].state == LogicState::high;
                const bool clockEdge = risingEdge
                                           ? (!previousClock && currentClock)
                                           : (previousClock && !currentClock);

                SlotState q = prevState.outputStates.empty() ? SlotState{} : prevState.outputStates[0];
                if (clear) {
                    q.state = LogicState::low;
                    q.lastChangeTime = simTime;
                } else if (clockEdge) {
                    q = inputs[0];
                    q.lastChangeTime = simTime;
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
            if (cellType == "$_DFF_P_" || cellType == "$dff") {
                return ensureDffDefinition("D Flip Flop", true);
            }
            if (cellType == "$_DFF_N_") {
                return ensureDffDefinition("D Flip Flop (Neg Edge)", false);
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

        class Importer {
          public:
            Importer(const Design &design, SimulationEngine &engine)
                : m_design(design), m_engine(engine) {}

            SimEngineImportResult importTop(const std::string &topModuleName) {
                const auto &topModule = *requireModule(topModuleName);

                m_result.topModuleName = topModuleName;
                m_result.top.definitionName = topModule.name;
                m_result.top.instancePath = topModuleName;
                m_result.instancesByPath[topModuleName] = m_result.top;

                PortBindings topBindings;
                initializeTopBoundary(topModule, topBindings);
                elaborateModule(topModule, topModuleName, topBindings);
                materializeConnections();

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

            std::string keyForBit(const SignalBit &bit) const {
                if (!bit.isNet()) {
                    throw std::runtime_error("Cannot build a net key for a constant signal bit");
                }
                return std::to_string(*bit.netId);
            }

            std::string scopedNetKey(std::string_view path, const SignalBit &bit) const {
                return std::string(path) + ":" + keyForBit(bit);
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

                const auto directPortIt = bindings.find(scopedNetKey(path, bit));
                if (directPortIt != bindings.end()) {
                    return directPortIt->second;
                }

                return SignalRef::net(scopedNetKey(path, bit));
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
                    if (port.direction == PortDirection::inout) {
                        throw std::runtime_error("Inout ports are not supported by the BESS Verilog importer");
                    }

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
                            const auto signal = SignalRef::net(scopedNetKey(topModule.name, port.bits[i]));
                            registerDriver(signal, SlotEndpoint{id, SlotType::digitalOutput, static_cast<int>(i)});
                        }
                    } else {
                        const auto id = createTopBoundaryComponent(outputDefinition,
                                                                   port.bits.size(),
                                                                   slotNames,
                                                                   false);
                        m_result.topOutputComponents[port.name] = id;
                        for (size_t i = 0; i < port.bits.size(); ++i) {
                            const auto signal = SignalRef::net(scopedNetKey(topModule.name, port.bits[i]));
                            registerLoad(signal, SlotEndpoint{id, SlotType::digitalInput, static_cast<int>(i)});
                        }
                    }
                }
            }

            void elaborateModule(const Module &module,
                                 const std::string &path,
                                 const PortBindings &bindings) {
                if (path != m_result.topModuleName) {
                    ImportedModuleInstance instance;
                    instance.definitionName = module.name;
                    instance.instancePath = path;
                    m_result.instancesByPath[path] = instance;
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
                                childBindings[scopedNetKey(path + "/" + cell.name, childPort.bits[i])] =
                                    resolveSignal(path, connIt->second[i], bindings);
                            }
                        }

                        elaborateModule(*childModule, path + "/" + cell.name, childBindings);
                        continue;
                    }

                    instantiatePrimitive(path, cell, bindings);
                }
            }

            void instantiatePrimitive(const std::string &path,
                                      const Cell &cell,
                                      const PortBindings &bindings) {
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
                        registerLoad(resolveSignal(path, inBits[i], bindings),
                                     SlotEndpoint{componentId, SlotType::digitalInput, 0});
                        registerDriver(resolveSignal(path, outBits[i], bindings),
                                       SlotEndpoint{componentId, SlotType::digitalOutput, 0});
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
                        registerLoad(resolveSignal(path, aBits[i], bindings),
                                     SlotEndpoint{componentId, SlotType::digitalInput, 0});
                        registerLoad(resolveSignal(path, bBits[i], bindings),
                                     SlotEndpoint{componentId, SlotType::digitalInput, 1});
                        registerDriver(resolveSignal(path, yBits[i], bindings),
                                       SlotEndpoint{componentId, SlotType::digitalOutput, 0});
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
                    auto component = m_engine.getDigitalComponent(componentId);
                    while (component->definition->getInputSlotsInfo().count < aBits.size()) {
                        component->incrementInputCount(true);
                    }
                    for (size_t i = 0; i < aBits.size(); ++i) {
                        registerLoad(resolveSignal(path, aBits[i], bindings),
                                     SlotEndpoint{componentId, SlotType::digitalInput, static_cast<int>(i)});
                    }
                    registerDriver(resolveSignal(path, yBits[0], bindings),
                                   SlotEndpoint{componentId, SlotType::digitalOutput, 0});
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
                        registerLoad(resolveSignal(path, aBits[i], bindings),
                                     SlotEndpoint{componentId, SlotType::digitalInput, 0});
                        registerLoad(resolveSignal(path, bBits[i], bindings),
                                     SlotEndpoint{componentId, SlotType::digitalInput, 1});
                        registerLoad(resolveSignal(path, sBits[0], bindings),
                                     SlotEndpoint{componentId, SlotType::digitalInput, 2});
                        registerDriver(resolveSignal(path, yBits[i], bindings),
                                       SlotEndpoint{componentId, SlotType::digitalOutput, 0});
                    }
                    return;
                }

                if (cell.type == "$_DFF_P_" || cell.type == "$_DFF_N_" || cell.type == "$dff") {
                    const auto &dBits = cell.connections.at("D");
                    const auto &qBits = cell.connections.at("Q");
                    const auto &clkBits = cell.connections.at(cell.connections.contains("C") ? "C" : "CLK");
                    if (clkBits.size() != 1 || dBits.size() != qBits.size()) {
                        throw std::runtime_error("Unsupported DFF width configuration in " + cell.name);
                    }
                    for (size_t i = 0; i < qBits.size(); ++i) {
                        const auto componentId = m_engine.addComponent(primitiveDefinition);
                        m_createdComponentIds.push_back(componentId);
                        registerLoad(resolveSignal(path, dBits[i], bindings),
                                     SlotEndpoint{componentId, SlotType::digitalInput, 0});
                        registerLoad(resolveSignal(path, clkBits[0], bindings),
                                     SlotEndpoint{componentId, SlotType::digitalInput, 1});
                        registerLoad(SignalRef::constantValue("0"),
                                     SlotEndpoint{componentId, SlotType::digitalInput, 2});
                        registerDriver(resolveSignal(path, qBits[i], bindings),
                                       SlotEndpoint{componentId, SlotType::digitalOutput, 0});
                    }
                    return;
                }

                throw std::runtime_error("Unsupported primitive cell type during import: " + cell.type);
            }

            void materializeConnections() {
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
        return importDesignIntoSimulationEngine(importVerilogToDesign(verilogFile, config),
                                                engine,
                                                config.topModuleName);
    }
} // namespace Bess::Verilog
