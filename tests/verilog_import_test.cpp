#include "application/pages/main_page/verilog_scene_import.h"
#include "bverilog/sim_engine_importer.h"
#include "bverilog/yosys_json_parser.h"
#include "bverilog/yosys_runner.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/scene_components/connection_scene_component.h"
#include "pages/main_page/scene_components/module_scene_component.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "scene/scene.h"
#include "simulation_engine.h"
#include "types.h"
#include "gtest/gtest.h"
#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <json/value.h>
#include <limits>
#include <memory>
#include <thread>
#include <unordered_map>

namespace {
    using Bess::UUID;
    using namespace Bess::SimEngine;
    using namespace Bess::Verilog;

    bool waitUntil(const std::function<bool()> &predicate,
                   std::chrono::milliseconds timeout = std::chrono::milliseconds(250),
                   std::chrono::milliseconds poll = std::chrono::milliseconds(2)) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (predicate()) {
                return true;
            }
            std::this_thread::sleep_for(poll);
        }
        return predicate();
    }

    std::filesystem::path writeTempVerilogFile(const std::string &fileName,
                                               const std::string &source) {
        const auto path = std::filesystem::temp_directory_path() / fileName;
        std::ofstream stream(path);
        if (!stream.is_open()) {
            throw std::runtime_error("Failed to create temporary Verilog test file");
        }
        stream << source;
        return path;
    }

    std::string buildUniqueTempVerilogFileName(const std::string &stem) {
        static std::atomic<uint64_t> counter{0};
        return std::format("{}_{}_{}.v",
                           stem,
                           std::chrono::steady_clock::now().time_since_epoch().count(),
                           counter.fetch_add(1));
    }
} // namespace

class VerilogImportTest : public testing::Test {
  protected:
    void SetUp() override {
        engine = &SimulationEngine::instance();
        engine->setSimulationState(SimulationState::running);
        engine->clear();
    }

    void TearDown() override {
        if (engine) {
            engine->setSimulationState(SimulationState::paused);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            engine->clear();
            engine = nullptr;
        }
    }

    Json::Value buildNestedModuleJson() {
        Json::Value root(Json::objectValue);
        root["modules"] = Json::Value(Json::objectValue);

        auto &child = root["modules"]["child"];
        child["attributes"]["top"] = "0";
        child["ports"]["a"]["direction"] = "input";
        child["ports"]["a"]["bits"].append(1);
        child["ports"]["b"]["direction"] = "input";
        child["ports"]["b"]["bits"].append(2);
        child["ports"]["y"]["direction"] = "output";
        child["ports"]["y"]["bits"].append(3);

        child["cells"]["invert"]["type"] = "$_NOT_";
        child["cells"]["invert"]["connections"]["A"].append(2);
        child["cells"]["invert"]["connections"]["Y"].append(4);
        child["cells"]["invert"]["port_directions"]["A"] = "input";
        child["cells"]["invert"]["port_directions"]["Y"] = "output";

        child["cells"]["combine"]["type"] = "$_AND_";
        child["cells"]["combine"]["connections"]["A"].append(1);
        child["cells"]["combine"]["connections"]["B"].append(4);
        child["cells"]["combine"]["connections"]["Y"].append(3);
        child["cells"]["combine"]["port_directions"]["A"] = "input";
        child["cells"]["combine"]["port_directions"]["B"] = "input";
        child["cells"]["combine"]["port_directions"]["Y"] = "output";

        auto &top = root["modules"]["top"];
        top["attributes"]["top"] = "1";
        top["ports"]["in0"]["direction"] = "input";
        top["ports"]["in0"]["bits"].append(10);
        top["ports"]["in1"]["direction"] = "input";
        top["ports"]["in1"]["bits"].append(11);
        top["ports"]["out0"]["direction"] = "output";
        top["ports"]["out0"]["bits"].append(12);

        top["cells"]["u_child"]["type"] = "child";
        top["cells"]["u_child"]["connections"]["a"].append(10);
        top["cells"]["u_child"]["connections"]["b"].append(11);
        top["cells"]["u_child"]["connections"]["y"].append(12);

        return root;
    }

    SimulationEngine *engine = nullptr;
};

TEST_F(VerilogImportTest, ParsesYosysJsonAndDetectsTopModule) {
    const auto design = parseDesignFromYosysJson(buildNestedModuleJson());
    ASSERT_EQ(design.topModuleName, "top");
    ASSERT_NE(design.findModule("child"), nullptr);
    ASSERT_NE(design.findModule("top"), nullptr);
}

TEST_F(VerilogImportTest, ImportsTopOutputWithConstantEncodedBit) {
    Json::Value root(Json::objectValue);
    root["modules"] = Json::Value(Json::objectValue);

    auto &top = root["modules"]["top"];
    top["attributes"]["top"] = "1";
    top["ports"]["const_out"]["direction"] = "output";
    top["ports"]["const_out"]["bits"].append("0");

    const auto design = parseDesignFromYosysJson(root);
    const auto result = importDesignIntoSimulationEngine(design, *engine);

    ASSERT_EQ(result.topModuleName, "top");
    ASSERT_TRUE(result.topOutputComponents.contains("const_out"));

    const auto constOut = result.topOutputComponents.at("const_out");
    ASSERT_TRUE(waitUntil([&] {
        return engine->getDigitalSlotState(constOut, SlotType::digitalInput, 0).state == LogicState::low;
    })) << "Constant-encoded top output did not resolve to low";
}

TEST_F(VerilogImportTest, ImportsNestedModulesIntoSimulationEngineAndPropagatesSignals) {
    const auto design = parseDesignFromYosysJson(buildNestedModuleJson());
    const auto result = importDesignIntoSimulationEngine(design, *engine);

    ASSERT_EQ(result.topModuleName, "top");
    ASSERT_TRUE(result.instancesByPath.contains("top"));
    ASSERT_TRUE(result.instancesByPath.contains("top/u_child"));
    EXPECT_EQ(result.instancesByPath.at("top/u_child").parentInstancePath, "top");
    ASSERT_TRUE(result.topInputComponents.contains("in0"));
    ASSERT_TRUE(result.topInputComponents.contains("in1"));
    ASSERT_TRUE(result.topOutputComponents.contains("out0"));
    EXPECT_EQ(result.componentInstancePathById.at(result.topInputComponents.at("in0")), "top");
    EXPECT_EQ(result.componentInstancePathById.at(result.topOutputComponents.at("out0")), "top");

    bool foundChildOwnedPrimitive = false;
    for (const auto &[componentId, ownerPath] : result.componentInstancePathById) {
        (void)componentId;
        if (ownerPath == "top/u_child") {
            foundChildOwnedPrimitive = true;
            break;
        }
    }
    EXPECT_TRUE(foundChildOwnedPrimitive);

    const auto in0 = result.topInputComponents.at("in0");
    const auto in1 = result.topInputComponents.at("in1");
    const auto out0 = result.topOutputComponents.at("out0");

    engine->setOutputSlotState(in0, 0, LogicState::high);
    engine->setOutputSlotState(in1, 0, LogicState::low);
    ASSERT_TRUE(waitUntil([&] {
        return engine->getDigitalSlotState(out0, SlotType::digitalInput, 0).state == LogicState::high;
    })) << "Expected output to resolve high for 1 & !0";

    engine->setOutputSlotState(in1, 0, LogicState::high);
    ASSERT_TRUE(waitUntil([&] {
        return engine->getDigitalSlotState(out0, SlotType::digitalInput, 0).state == LogicState::low;
    })) << "Expected output to resolve low for 1 & !1";
}

TEST_F(VerilogImportTest, PreservesHierarchicalHalfAdderInstanceInterfacesForSceneImport) {
    const auto verilogPath = writeTempVerilogFile(
        "bess_hierarchical_full_adder_test.v",
        R"verilog(
module full_add(a,b,cin,sum,cout);
  input a,b,cin;
  output sum,cout;
  wire x,y,z;

  half_add h1(.a(a),.b(b),.s(x),.c(y));
  half_add h2(.a(x),.b(cin),.s(sum),.c(z));
  or o1(cout,y,z);
endmodule : full_add

module half_add(a,b,s,c);
  input a,b;
  output s,c;

  xor x1(s,a,b);
  and a1(c,a,b);
endmodule :half_add
)verilog");

    const auto result = importVerilogFileIntoSimulationEngine(
        verilogPath,
        *engine,
        YosysRunnerConfig{
            .executablePath = "yosys",
            .topModuleName = std::string("full_add"),
        });

    ASSERT_TRUE(result.instancesByPath.contains("full_add/h1"));
    ASSERT_TRUE(result.instancesByPath.contains("full_add/h2"));

    const auto &h1 = result.instancesByPath.at("full_add/h1");
    const auto &h2 = result.instancesByPath.at("full_add/h2");

    EXPECT_EQ(h1.definitionName, "half_add");
    EXPECT_EQ(h2.definitionName, "half_add");
    EXPECT_EQ(h1.parentInstancePath, "full_add");
    EXPECT_EQ(h2.parentInstancePath, "full_add");
    EXPECT_EQ(h1.inputSlotNames, (std::vector<std::string>{"a", "b"}));
    EXPECT_EQ(h2.inputSlotNames, (std::vector<std::string>{"a", "b"}));
    EXPECT_EQ(h1.outputSlotNames, (std::vector<std::string>{"s", "c"}));
    EXPECT_EQ(h2.outputSlotNames, (std::vector<std::string>{"s", "c"}));
    ASSERT_EQ(h1.internalInputSinks.size(), 2u);
    ASSERT_EQ(h1.internalOutputDrivers.size(), 2u);
    ASSERT_EQ(h2.internalInputSinks.size(), 2u);
    ASSERT_EQ(h2.internalOutputDrivers.size(), 2u);
    EXPECT_FALSE(h1.internalInputSinks[0].empty());
    EXPECT_FALSE(h1.internalInputSinks[1].empty());
    EXPECT_FALSE(h1.internalOutputDrivers[0].empty());
    EXPECT_FALSE(h1.internalOutputDrivers[1].empty());
    EXPECT_FALSE(h2.internalInputSinks[0].empty());
    EXPECT_FALSE(h2.internalInputSinks[1].empty());
    EXPECT_FALSE(h2.internalOutputDrivers[0].empty());
    EXPECT_FALSE(h2.internalOutputDrivers[1].empty());

    std::filesystem::remove(verilogPath);
}

TEST_F(VerilogImportTest, ImportsVerilogFullAdderViaYosysAndMatchesTruthTable) {
    const auto verilogPath = writeTempVerilogFile(
        "bess_full_adder_test.v",
        R"verilog(
module full_adder(
    input a,
    input b,
    input cin,
    output sum,
    output cout
);
    wire axb;
    wire ab;
    wire cin_axb;

    assign axb = a ^ b;
    assign sum = axb ^ cin;
    assign ab = a & b;
    assign cin_axb = cin & axb;
    assign cout = ab | cin_axb;
endmodule
)verilog");

    const auto result = importVerilogFileIntoSimulationEngine(
        verilogPath,
        *engine,
        YosysRunnerConfig{
            .executablePath = "yosys",
            .topModuleName = std::string("full_adder"),
        });

    ASSERT_EQ(result.topModuleName, "full_adder");
    ASSERT_TRUE(result.topInputComponents.contains("a"));
    ASSERT_TRUE(result.topInputComponents.contains("b"));
    ASSERT_TRUE(result.topInputComponents.contains("cin"));
    ASSERT_TRUE(result.topOutputComponents.contains("sum"));
    ASSERT_TRUE(result.topOutputComponents.contains("cout"));

    const auto a = result.topInputComponents.at("a");
    const auto b = result.topInputComponents.at("b");
    const auto cin = result.topInputComponents.at("cin");
    const auto sum = result.topOutputComponents.at("sum");
    const auto cout = result.topOutputComponents.at("cout");

    const std::array<std::array<int, 5>, 8> truthTable = {{
        {0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0},
        {0, 1, 0, 1, 0},
        {0, 1, 1, 0, 1},
        {1, 0, 0, 1, 0},
        {1, 0, 1, 0, 1},
        {1, 1, 0, 0, 1},
        {1, 1, 1, 1, 1},
    }};

    auto asLogic = [](int bit) {
        return bit ? LogicState::high : LogicState::low;
    };

    for (const auto &row : truthTable) {
        engine->setOutputSlotState(a, 0, asLogic(row[0]));
        engine->setOutputSlotState(b, 0, asLogic(row[1]));
        engine->setOutputSlotState(cin, 0, asLogic(row[2]));

        ASSERT_TRUE(waitUntil([&] {
            return engine->getDigitalSlotState(sum, SlotType::digitalInput, 0).state == asLogic(row[3]) &&
                   engine->getDigitalSlotState(cout, SlotType::digitalInput, 0).state == asLogic(row[4]);
        })) << "Full adder outputs did not settle for inputs "
            << row[0] << row[1] << row[2];
    }

    std::filesystem::remove(verilogPath);
}

TEST_F(VerilogImportTest, ImportedBoundaryInputCanDriveRecordedInternalPrimitiveSinks) {
    const auto verilogPath = writeTempVerilogFile(
        "bess_hierarchical_bridge_test.v",
        R"verilog(
module full_add(a,b,cin,sum,cout);
  input a,b,cin;
  output sum,cout;
  wire x,y,z;

  half_add h1(.a(a),.b(b),.s(x),.c(y));
  half_add h2(.a(x),.b(cin),.s(sum),.c(z));
  or o1(cout,y,z);
endmodule : full_add

module half_add(a,b,s,c);
  input a,b;
  output s,c;

  xor x1(s,a,b);
  and a1(c,a,b);
endmodule :half_add
)verilog");

    const auto result = importVerilogFileIntoSimulationEngine(
        verilogPath,
        *engine,
        YosysRunnerConfig{
            .executablePath = "yosys",
            .topModuleName = std::string("full_add"),
        });

    const auto &h1 = result.instancesByPath.at("full_add/h1");
    ASSERT_EQ(h1.inputSlotNames, (std::vector<std::string>{"a", "b"}));
    ASSERT_EQ(h1.internalInputSinks.size(), 2u);

    auto resizeOutputs = [](const std::shared_ptr<Drivers::Digital::DigSimComp> &component,
                            size_t count) {
        auto def = component->getDefinition<Drivers::Digital::DigCompDef>();
        ASSERT_NE(def, nullptr);

        auto outInfo = def->getOutputSlotsInfo();
        if (outInfo.count < count) {
            outInfo.count = count;
            def->setOutputSlotsInfo(outInfo);
        }

        component->getOutputStates().resize(count);
        component->getOutputConnections().resize(count);
        component->getIsOutputConnected().resize(count, false);
    };

    const auto topA = result.topInputComponents.at("a");
    const auto inputDefinition = engine->getComponentDefinition(topA);
    const auto bridgeInputId = engine->addComponent(inputDefinition);
    const auto bridgeInput = engine->getComponent<Drivers::Digital::DigSimComp>(bridgeInputId);
    resizeOutputs(bridgeInput, h1.inputSlotNames.size());

    for (const auto &sink : h1.internalInputSinks[0]) {
        const auto connections = engine->getConnections(sink.componentId);
        ASSERT_LT(sink.slotIndex, static_cast<int>(connections.inputs.size()));
        for (const auto &[srcId, srcSlot] : connections.inputs[sink.slotIndex]) {
            if (srcId == topA) {
                engine->deleteConnection(topA,
                                         SlotType::digitalOutput,
                                         srcSlot,
                                         sink.componentId,
                                         sink.slotType,
                                         sink.slotIndex);
            }
        }

        ASSERT_TRUE(engine->connectComponent(bridgeInputId,
                                             0,
                                             SlotType::digitalOutput,
                                             sink.componentId,
                                             sink.slotIndex,
                                             sink.slotType));
    }

    engine->setOutputSlotState(bridgeInputId, 0, LogicState::high);

    ASSERT_TRUE(waitUntil([&] {
        for (const auto &sink : h1.internalInputSinks[0]) {
            const auto aggregatedInputs = engine->getInputSlotsState(sink.componentId);
            if (static_cast<size_t>(sink.slotIndex) >= aggregatedInputs.size() ||
                aggregatedInputs[sink.slotIndex].state != LogicState::high) {
                return false;
            }
        }
        return true;
    })) << "Recorded internal primitive sinks did not resolve high from the bridged module input";

    ASSERT_TRUE(waitUntil([&] {
        for (const auto &sink : h1.internalInputSinks[0]) {
            if (engine->getDigitalSlotState(sink.componentId, SlotType::digitalInput, sink.slotIndex).state !=
                LogicState::high) {
                return false;
            }
        }
        return true;
    })) << "Internal primitive sinks did not receive bridged module input state";

    std::filesystem::remove(verilogPath);
}

TEST_F(VerilogImportTest, ImportsVectorPortsAndCarryOutputFromAluVerilog) {
    const auto verilogPath = writeTempVerilogFile(
        "bess_alu_test.v",
        R"verilog(
module alu(
    input [7:0] A,
    input [7:0] B,
    input [3:0] ALU_Sel,
    output [7:0] ALU_Out,
    output CarryOut
);
    reg [7:0] ALU_Result;
    wire [8:0] tmp;

    assign ALU_Out = ALU_Result;
    assign tmp = {1'b0, A} + {1'b0, B};
    assign CarryOut = tmp[8];

    always @(*) begin
        case (ALU_Sel)
            4'b0000: ALU_Result = A + B;
            4'b1000: ALU_Result = A & B;
            default: ALU_Result = 8'h00;
        endcase
    end
endmodule
)verilog");

    const auto result = importVerilogFileIntoSimulationEngine(
        verilogPath,
        *engine,
        YosysRunnerConfig{
            .executablePath = "yosys",
            .topModuleName = std::string("alu"),
        });

    ASSERT_EQ(result.topModuleName, "alu");
    ASSERT_TRUE(result.topInputComponents.contains("A"));
    ASSERT_TRUE(result.topInputComponents.contains("B"));
    ASSERT_TRUE(result.topInputComponents.contains("ALU_Sel"));
    ASSERT_TRUE(result.topOutputComponents.contains("ALU_Out"));
    ASSERT_TRUE(result.topOutputComponents.contains("CarryOut"));

    const auto a = result.topInputComponents.at("A");
    const auto b = result.topInputComponents.at("B");
    const auto aluSel = result.topInputComponents.at("ALU_Sel");
    const auto aluOut = result.topOutputComponents.at("ALU_Out");
    const auto carryOut = result.topOutputComponents.at("CarryOut");

    EXPECT_EQ(engine->getComponent<Drivers::Digital::DigSimComp>(a)->getDefinition<Drivers::Digital::DigCompDef>()->getOutputSlotsInfo().count, 8);
    EXPECT_EQ(engine->getComponent<Drivers::Digital::DigSimComp>(b)->getDefinition<Drivers::Digital::DigCompDef>()->getOutputSlotsInfo().count, 8);
    EXPECT_EQ(engine->getComponent<Drivers::Digital::DigSimComp>(aluSel)->getDefinition<Drivers::Digital::DigCompDef>()->getOutputSlotsInfo().count, 4);
    EXPECT_EQ(engine->getComponent<Drivers::Digital::DigSimComp>(aluOut)->getDefinition<Drivers::Digital::DigCompDef>()->getInputSlotsInfo().count, 8);
    EXPECT_EQ(engine->getComponent<Drivers::Digital::DigSimComp>(carryOut)->getDefinition<Drivers::Digital::DigCompDef>()->getInputSlotsInfo().count, 1);

    auto writeBus = [&](const UUID &componentId, uint32_t value, size_t width) {
        for (size_t i = 0; i < width; ++i) {
            const auto bit = ((value >> i) & 1U) != 0U ? LogicState::high : LogicState::low;
            engine->setOutputSlotState(componentId, static_cast<int>(i), bit);
        }
    };

    auto readBus = [&](const UUID &componentId, size_t width) {
        uint32_t value = 0;
        for (size_t i = 0; i < width; ++i) {
            if (engine->getDigitalSlotState(componentId, SlotType::digitalInput, static_cast<int>(i)).state ==
                LogicState::high) {
                value |= (1U << i);
            }
        }
        return value;
    };

    writeBus(a, 0xFF, 8);
    writeBus(b, 0x01, 8);
    writeBus(aluSel, 0x0, 4);
    ASSERT_TRUE(waitUntil([&] {
        return readBus(aluOut, 8) == 0x00 &&
               engine->getDigitalSlotState(carryOut, SlotType::digitalInput, 0).state == LogicState::high;
    })) << "ALU add mode did not produce expected overflow output";

    writeBus(a, 0xAA, 8);
    writeBus(b, 0xCC, 8);
    writeBus(aluSel, 0x8, 4);
    ASSERT_TRUE(waitUntil([&] {
        return readBus(aluOut, 8) == 0x88 &&
               engine->getDigitalSlotState(carryOut, SlotType::digitalInput, 0).state == LogicState::high;
    })) << "ALU and mode did not preserve carry output wiring";

    std::filesystem::remove(verilogPath);
}

TEST_F(VerilogImportTest, PopulatesSceneWithBothAluOutputsIncludingCarryOut) {
    const auto verilogPath = writeTempVerilogFile(
        "bess_alu_scene_test.v",
        R"verilog(
module alu(
    input [7:0] A,
    input [7:0] B,
    input [3:0] ALU_Sel,
    output [7:0] ALU_Out,
    output CarryOut
);
    reg [7:0] ALU_Result;
    wire [8:0] tmp;

    assign ALU_Out = ALU_Result;
    assign tmp = {1'b0, A} + {1'b0, B};
    assign CarryOut = tmp[8];

    always @(*) begin
        case (ALU_Sel)
            4'b0000: ALU_Result = A + B;
            default: ALU_Result = 8'h00;
        endcase
    end
endmodule
)verilog");

    const auto result = importVerilogFileIntoSimulationEngine(
        verilogPath,
        *engine,
        YosysRunnerConfig{
            .executablePath = "yosys",
            .topModuleName = std::string("alu"),
        });

    auto scene = Bess::Pages::MainPage::getInstance()->getState().getSceneDriver().getActiveScene();
    ASSERT_NE(scene, nullptr);
    scene->clear();
    Bess::Pages::populateSceneFromVerilogImportResult(result, *engine, *scene);

    bool foundAluOut = false;
    bool foundCarryOut = false;
    const auto &sceneState = scene->getState();

    auto countRealInputSlots = [&](const std::shared_ptr<Bess::Canvas::SimulationSceneComponent> &simComp) {
        size_t count = 0;
        for (const auto &slotId : simComp->getInputSlots()) {
            const auto slot = sceneState.getComponentByUuid<Bess::Canvas::SlotSceneComponent>(slotId);
            if (slot && !slot->isResizeSlot()) {
                ++count;
            }
        }
        return count;
    };

    for (const auto &[uuid, component] : sceneState.getAllComponents()) {
        (void)uuid;
        if (component->getType() != Bess::Canvas::SceneComponentType::simulation) {
            continue;
        }
        const auto simComp = component->cast<Bess::Canvas::SimulationSceneComponent>();
        if (!simComp) {
            continue;
        }

        if (simComp->getName() == "ALU_Out") {
            foundAluOut = true;
            EXPECT_EQ(countRealInputSlots(simComp), 8u);
        }

        if (simComp->getName() == "CarryOut") {
            foundCarryOut = true;
            EXPECT_EQ(countRealInputSlots(simComp), 1u);
        }
    }

    EXPECT_TRUE(foundAluOut);
    EXPECT_TRUE(foundCarryOut);

    std::filesystem::remove(verilogPath);
}

TEST_F(VerilogImportTest, AppliesHierarchicalLayoutToImportedRootAndModuleScenes) {
    const auto verilogPath = writeTempVerilogFile(
        "bess_hierarchical_layout_scene_test.v",
        R"verilog(
module full_add(a,b,cin,sum,cout);
  input a,b,cin;
  output sum,cout;
  wire x,y,z;

  half_add h1(.a(a),.b(b),.s(x),.c(y));
  half_add h2(.a(x),.b(cin),.s(sum),.c(z));
  or o1(cout,y,z);
endmodule : full_add

module half_add(a,b,s,c);
  input a,b;
  output s,c;

  xor x1(s,a,b);
  and a1(c,a,b);
endmodule :half_add
)verilog");

    const auto result = importVerilogFileIntoSimulationEngine(
        verilogPath,
        *engine,
        YosysRunnerConfig{
            .executablePath = "yosys",
            .topModuleName = std::string("full_add"),
        });

    auto scene = Bess::Pages::MainPage::getInstance()->getState().getSceneDriver().getActiveScene();
    ASSERT_NE(scene, nullptr);
    scene->clear();
    Bess::Pages::populateSceneFromVerilogImportResult(result, *engine, *scene);

    std::vector<std::shared_ptr<Bess::Canvas::SimulationSceneComponent>> rootInputs;
    std::vector<std::shared_ptr<Bess::Canvas::SimulationSceneComponent>> rootOutputs;
    std::shared_ptr<Bess::Canvas::ModuleSceneComponent> topModule;

    for (const auto &[uuid, component] : scene->getState().getAllComponents()) {
        (void)uuid;
        if (component->getParentComponent() != UUID::null) {
            continue;
        }

        if (component->getType() == Bess::Canvas::SceneComponentType::module) {
            const auto moduleComponent =
                std::dynamic_pointer_cast<Bess::Canvas::ModuleSceneComponent>(component);
            if (moduleComponent && moduleComponent->getName() == "full_add") {
                topModule = moduleComponent;
            }
            continue;
        }

        if (component->getType() != Bess::Canvas::SceneComponentType::simulation) {
            continue;
        }

        const auto simComponent =
            std::dynamic_pointer_cast<Bess::Canvas::SimulationSceneComponent>(component);
        if (!simComponent) {
            continue;
        }

        if (simComponent->getName() == "a" ||
            simComponent->getName() == "b" ||
            simComponent->getName() == "cin") {
            rootInputs.push_back(simComponent);
        } else if (simComponent->getName() == "sum" ||
                   simComponent->getName() == "cout") {
            rootOutputs.push_back(simComponent);
        }
    }

    ASSERT_EQ(rootInputs.size(), 3u);
    ASSERT_EQ(rootOutputs.size(), 2u);
    ASSERT_NE(topModule, nullptr);

    float maxInputX = std::numeric_limits<float>::lowest();
    for (const auto &input : rootInputs) {
        maxInputX = std::max(maxInputX, input->getTransform().position.x);
    }

    const auto topModuleX = topModule->getTransform().position.x;

    float minOutputX = std::numeric_limits<float>::max();
    for (const auto &output : rootOutputs) {
        minOutputX = std::min(minOutputX, output->getTransform().position.x);
    }

    EXPECT_LE(maxInputX, topModuleX);
    EXPECT_LT(topModuleX, minOutputX);

    auto &sceneDriver = Bess::Pages::MainPage::getInstance()->getState().getSceneDriver();
    const auto topModuleScene = sceneDriver.getSceneWithId(topModule->getSceneId());
    ASSERT_NE(topModuleScene, nullptr);

    std::shared_ptr<Bess::Canvas::SimulationSceneComponent> moduleInput;
    std::shared_ptr<Bess::Canvas::SimulationSceneComponent> moduleOutput;
    std::vector<std::shared_ptr<Bess::Canvas::ModuleSceneComponent>> childModules;
    float minInternalX = std::numeric_limits<float>::max();
    float maxInternalX = std::numeric_limits<float>::lowest();
    size_t internalCount = 0;

    for (const auto &[uuid, component] : topModuleScene->getState().getAllComponents()) {
        (void)uuid;

        if (component->getParentComponent() != UUID::null) {
            continue;
        }

        if (component->getType() == Bess::Canvas::SceneComponentType::module) {
            const auto moduleComponent =
                std::dynamic_pointer_cast<Bess::Canvas::ModuleSceneComponent>(component);
            if (moduleComponent &&
                (moduleComponent->getName() == "h1" || moduleComponent->getName() == "h2")) {
                childModules.push_back(moduleComponent);
            }
            continue;
        }

        if (component->getType() != Bess::Canvas::SceneComponentType::simulation) {
            continue;
        }

        const auto simComponent =
            std::dynamic_pointer_cast<Bess::Canvas::SimulationSceneComponent>(component);
        if (!simComponent) {
            continue;
        }

        if (simComponent->getName() == "Module Input") {
            moduleInput = simComponent;
            continue;
        }

        if (simComponent->getName() == "Module Output") {
            moduleOutput = simComponent;
            continue;
        }

        minInternalX = std::min(minInternalX, simComponent->getTransform().position.x);
        maxInternalX = std::max(maxInternalX, simComponent->getTransform().position.x);
        ++internalCount;
    }

    ASSERT_NE(moduleInput, nullptr);
    ASSERT_NE(moduleOutput, nullptr);
    ASSERT_EQ(childModules.size(), 2u);
    ASSERT_GT(internalCount, 0u);
    EXPECT_LT(moduleInput->getTransform().position.x, minInternalX);
    EXPECT_LT(maxInternalX, moduleOutput->getTransform().position.x);

    std::filesystem::remove(verilogPath);
}

TEST_F(VerilogImportTest, SetsParentSceneAndModuleOwnershipForImportedSubmodules) {
    const auto verilogPath = writeTempVerilogFile(
        "bess_hierarchical_parent_scene_test.v",
        R"verilog(
module full_add(a,b,cin,sum,cout);
  input a,b,cin;
  output sum,cout;
  wire x,y,z;

  half_add h1(.a(a),.b(b),.s(x),.c(y));
  half_add h2(.a(x),.b(cin),.s(sum),.c(z));
  or o1(cout,y,z);
endmodule : full_add

module half_add(a,b,s,c);
  input a,b;
  output s,c;

  xor x1(s,a,b);
  and a1(c,a,b);
endmodule :half_add
)verilog");

    const auto result = importVerilogFileIntoSimulationEngine(
        verilogPath,
        *engine,
        YosysRunnerConfig{
            .executablePath = "yosys",
            .topModuleName = std::string("full_add"),
        });

    auto &sceneDriver = Bess::Pages::MainPage::getInstance()->getState().getSceneDriver();
    auto rootScene = sceneDriver.getActiveScene();
    ASSERT_NE(rootScene, nullptr);
    rootScene->clear();
    Bess::Pages::populateSceneFromVerilogImportResult(result, *engine, *rootScene);

    std::shared_ptr<Bess::Canvas::ModuleSceneComponent> topModule;
    bool foundTopAsRootComponent = false;

    for (const auto &[uuid, component] : rootScene->getState().getAllComponents()) {
        (void)uuid;
        if (component->getType() != Bess::Canvas::SceneComponentType::module) {
            continue;
        }

        const auto moduleComponent = std::dynamic_pointer_cast<Bess::Canvas::ModuleSceneComponent>(component);
        if (!moduleComponent || moduleComponent->getName() != "full_add") {
            continue;
        }

        topModule = moduleComponent;
        foundTopAsRootComponent = (moduleComponent->getParentComponent() == UUID::null);
        break;
    }

    ASSERT_NE(topModule, nullptr);
    EXPECT_TRUE(foundTopAsRootComponent);

    const auto topModuleScene = sceneDriver.getSceneWithId(topModule->getSceneId());
    ASSERT_NE(topModuleScene, nullptr);
    EXPECT_FALSE(topModuleScene->getState().getIsRootScene());
    EXPECT_EQ(topModuleScene->getState().getParentSceneId(), rootScene->getState().getSceneId());
    EXPECT_EQ(topModuleScene->getState().getModuleId(), topModule->getUuid());

    std::vector<std::shared_ptr<Bess::Canvas::ModuleSceneComponent>> childModules;
    for (const auto &[uuid, component] : topModuleScene->getState().getAllComponents()) {
        (void)uuid;
        if (component->getType() != Bess::Canvas::SceneComponentType::module) {
            continue;
        }

        const auto moduleComponent = std::dynamic_pointer_cast<Bess::Canvas::ModuleSceneComponent>(component);
        if (!moduleComponent) {
            continue;
        }

        if (moduleComponent->getName() == "h1" || moduleComponent->getName() == "h2") {
            childModules.push_back(moduleComponent);
        }
    }

    ASSERT_EQ(childModules.size(), 2u);

    for (const auto &childModule : childModules) {
        ASSERT_NE(childModule, nullptr);
        EXPECT_EQ(childModule->getParentComponent(), UUID::null);

        const auto childModuleScene = sceneDriver.getSceneWithId(childModule->getSceneId());
        ASSERT_NE(childModuleScene, nullptr);
        EXPECT_FALSE(childModuleScene->getState().getIsRootScene());
        EXPECT_EQ(childModuleScene->getState().getParentSceneId(), topModuleScene->getState().getSceneId());
        EXPECT_EQ(childModuleScene->getState().getModuleId(), childModule->getUuid());
    }

    std::filesystem::remove(verilogPath);
}

TEST_F(VerilogImportTest, RoutesTopBoundaryViaModuleBridgeAndBuildsBoundarySceneConnections) {
    const auto verilogPath = writeTempVerilogFile(
        "bess_hierarchical_boundary_routing_test.v",
        R"verilog(
module full_add(a,b,cin,sum,cout);
  input a,b,cin;
  output sum,cout;
  wire x,y,z;

  half_add h1(.a(a),.b(b),.s(x),.c(y));
  half_add h2(.a(x),.b(cin),.s(sum),.c(z));
  or o1(cout,y,z);
endmodule : full_add

module half_add(a,b,s,c);
  input a,b;
  output s,c;

  xor x1(s,a,b);
  and a1(c,a,b);
endmodule :half_add
)verilog");

    const auto result = importVerilogFileIntoSimulationEngine(
        verilogPath,
        *engine,
        YosysRunnerConfig{
            .executablePath = "yosys",
            .topModuleName = std::string("full_add"),
        });

    auto &sceneDriver = Bess::Pages::MainPage::getInstance()->getState().getSceneDriver();
    auto rootScene = sceneDriver.getActiveScene();
    ASSERT_NE(rootScene, nullptr);
    rootScene->clear();
    Bess::Pages::populateSceneFromVerilogImportResult(result, *engine, *rootScene);

    auto &rootState = rootScene->getState();

    std::shared_ptr<Bess::Canvas::ModuleSceneComponent> topModule;
    std::unordered_map<UUID, std::shared_ptr<Bess::Canvas::SimulationSceneComponent>> rootSimById;
    for (const auto &[uuid, component] : rootState.getAllComponents()) {
        (void)uuid;
        if (component->getType() == Bess::Canvas::SceneComponentType::module) {
            const auto moduleComp = std::dynamic_pointer_cast<Bess::Canvas::ModuleSceneComponent>(component);
            if (moduleComp && moduleComp->getName() == "full_add") {
                topModule = moduleComp;
            }
            continue;
        }

        if (component->getType() != Bess::Canvas::SceneComponentType::simulation) {
            continue;
        }

        const auto simComp = std::dynamic_pointer_cast<Bess::Canvas::SimulationSceneComponent>(component);
        if (!simComp) {
            continue;
        }
        rootSimById[simComp->getSimEngineId()] = simComp;
    }

    ASSERT_NE(topModule, nullptr);
    ASSERT_TRUE(result.topInputComponents.contains("a"));
    ASSERT_TRUE(result.topInputComponents.contains("b"));
    ASSERT_TRUE(result.topInputComponents.contains("cin"));
    ASSERT_TRUE(result.topOutputComponents.contains("sum"));
    ASSERT_TRUE(result.topOutputComponents.contains("cout"));

    const auto topA = result.topInputComponents.at("a");
    const auto topB = result.topInputComponents.at("b");
    const auto topCin = result.topInputComponents.at("cin");
    const auto topSum = result.topOutputComponents.at("sum");
    const auto topCout = result.topOutputComponents.at("cout");
    const auto topWrapperId = topModule->getSimEngineId();

    const auto topModuleScene = sceneDriver.getSceneWithId(topModule->getSceneId());
    ASSERT_NE(topModuleScene, nullptr);
    auto &topModuleState = topModuleScene->getState();

    std::shared_ptr<Bess::Canvas::SimulationSceneComponent> moduleInputSceneComp;
    std::shared_ptr<Bess::Canvas::SimulationSceneComponent> moduleOutputSceneComp;
    std::unordered_map<std::string, std::shared_ptr<Bess::Canvas::ModuleSceneComponent>> childModules;
    for (const auto &[uuid, component] : topModuleState.getAllComponents()) {
        (void)uuid;
        if (component->getType() == Bess::Canvas::SceneComponentType::module) {
            const auto moduleComp = std::dynamic_pointer_cast<Bess::Canvas::ModuleSceneComponent>(component);
            if (moduleComp && (moduleComp->getName() == "h1" || moduleComp->getName() == "h2")) {
                childModules[moduleComp->getName()] = moduleComp;
            }
            continue;
        }

        if (component->getType() != Bess::Canvas::SceneComponentType::simulation) {
            continue;
        }

        const auto simComp = std::dynamic_pointer_cast<Bess::Canvas::SimulationSceneComponent>(component);
        if (!simComp) {
            continue;
        }

        if (simComp->getName() == "Module Input") {
            moduleInputSceneComp = simComp;
        } else if (simComp->getName() == "Module Output") {
            moduleOutputSceneComp = simComp;
        }
    }

    ASSERT_NE(moduleInputSceneComp, nullptr);
    ASSERT_NE(moduleOutputSceneComp, nullptr);
    ASSERT_TRUE(childModules.contains("h1"));
    ASSERT_TRUE(childModules.contains("h2"));

    const auto moduleInputId = moduleInputSceneComp->getSimEngineId();
    const auto moduleOutputId = moduleOutputSceneComp->getSimEngineId();
    const auto h1Id = childModules.at("h1")->getSimEngineId();
    const auto h2Id = childModules.at("h2")->getSimEngineId();

    auto hasEdge = [&](const std::vector<std::vector<std::pair<UUID, int>>> &buckets,
                       int bucketIndex,
                       UUID nodeId,
                       int nodeSlot) {
        if (bucketIndex < 0 || bucketIndex >= static_cast<int>(buckets.size())) {
            return false;
        }
        for (const auto &[id, slot] : buckets[bucketIndex]) {
            if (id == nodeId && slot == nodeSlot) {
                return true;
            }
        }
        return false;
    };

    auto isInternalToTopModule = [&](UUID simId) {
        if (simId == h1Id || simId == h2Id) {
            return true;
        }
        if (simId == topA || simId == topB || simId == topCin || simId == topSum || simId == topCout) {
            return false;
        }

        const auto it = result.componentInstancePathById.find(simId);
        if (it == result.componentInstancePathById.end()) {
            return false;
        }
        return it->second == "full_add" || it->second.starts_with("full_add/");
    };

    const auto aConnections = engine->getConnections(topA);
    ASSERT_EQ(aConnections.outputs.size(), 1u);
    EXPECT_TRUE(hasEdge(aConnections.outputs, 0, topWrapperId, 0));
    for (const auto &[dstId, dstSlot] : aConnections.outputs[0]) {
        EXPECT_EQ(dstId, topWrapperId);
        EXPECT_EQ(dstSlot, 0);
    }

    const auto bConnections = engine->getConnections(topB);
    ASSERT_EQ(bConnections.outputs.size(), 1u);
    EXPECT_TRUE(hasEdge(bConnections.outputs, 0, topWrapperId, 1));
    for (const auto &[dstId, dstSlot] : bConnections.outputs[0]) {
        EXPECT_EQ(dstId, topWrapperId);
        EXPECT_EQ(dstSlot, 1);
    }

    const auto cinConnections = engine->getConnections(topCin);
    ASSERT_EQ(cinConnections.outputs.size(), 1u);
    EXPECT_TRUE(hasEdge(cinConnections.outputs, 0, topWrapperId, 2));
    for (const auto &[dstId, dstSlot] : cinConnections.outputs[0]) {
        EXPECT_EQ(dstId, topWrapperId);
        EXPECT_EQ(dstSlot, 2);
    }

    const auto sumConnections = engine->getConnections(topSum);
    ASSERT_EQ(sumConnections.inputs.size(), 1u);
    EXPECT_TRUE(hasEdge(sumConnections.inputs, 0, topWrapperId, 0));
    for (const auto &[srcId, srcSlot] : sumConnections.inputs[0]) {
        EXPECT_EQ(srcId, topWrapperId);
        EXPECT_EQ(srcSlot, 0);
    }

    const auto coutConnections = engine->getConnections(topCout);
    ASSERT_EQ(coutConnections.inputs.size(), 1u);
    EXPECT_TRUE(hasEdge(coutConnections.inputs, 0, topWrapperId, 1));
    for (const auto &[srcId, srcSlot] : coutConnections.inputs[0]) {
        EXPECT_EQ(srcId, topWrapperId);
        EXPECT_EQ(srcSlot, 1);
    }

    const auto moduleInputConnections = engine->getConnections(moduleInputId);
    ASSERT_GE(moduleInputConnections.outputs.size(), 3u);
    EXPECT_TRUE(hasEdge(moduleInputConnections.outputs, 0, h1Id, 0));
    EXPECT_TRUE(hasEdge(moduleInputConnections.outputs, 1, h1Id, 1));
    EXPECT_TRUE(hasEdge(moduleInputConnections.outputs, 2, h2Id, 1));

    const auto moduleOutputConnections = engine->getConnections(moduleOutputId);
    ASSERT_GE(moduleOutputConnections.inputs.size(), 2u);
    ASSERT_FALSE(moduleOutputConnections.inputs[0].empty());
    ASSERT_FALSE(moduleOutputConnections.inputs[1].empty());
    EXPECT_TRUE(hasEdge(moduleOutputConnections.inputs, 0, h2Id, 0));

    for (const auto &[srcId, srcSlot] : moduleOutputConnections.inputs[0]) {
        (void)srcSlot;
        EXPECT_TRUE(isInternalToTopModule(srcId));
    }
    for (const auto &[srcId, srcSlot] : moduleOutputConnections.inputs[1]) {
        (void)srcSlot;
        EXPECT_TRUE(isInternalToTopModule(srcId));
    }

    auto findNamedSlot = [](const Bess::Canvas::SceneState &state,
                            const std::vector<UUID> &slotIds,
                            const std::string &slotName) {
        for (const auto &slotId : slotIds) {
            const auto slot = state.getComponentByUuid<Bess::Canvas::SlotSceneComponent>(slotId);
            if (!slot || slot->isResizeSlot()) {
                continue;
            }
            if (slot->getName() == slotName) {
                return slotId;
            }
        }
        return UUID::null;
    };

    auto firstRealSlot = [](const Bess::Canvas::SceneState &state,
                            const std::vector<UUID> &slotIds) {
        for (const auto &slotId : slotIds) {
            const auto slot = state.getComponentByUuid<Bess::Canvas::SlotSceneComponent>(slotId);
            if (slot && !slot->isResizeSlot()) {
                return slotId;
            }
        }
        return UUID::null;
    };

    auto hasSceneConnection = [](const Bess::Canvas::SceneState &state,
                                 const UUID &startSlot,
                                 const UUID &endSlot) {
        for (const auto &[uuid, component] : state.getAllComponents()) {
            (void)uuid;
            if (component->getType() != Bess::Canvas::SceneComponentType::connection) {
                continue;
            }

            const auto conn = std::dynamic_pointer_cast<Bess::Canvas::ConnectionSceneComponent>(component);
            if (conn && conn->getStartSlot() == startSlot && conn->getEndSlot() == endSlot) {
                return true;
            }
        }
        return false;
    };

    const auto aComp = rootSimById.at(topA);
    const auto bComp = rootSimById.at(topB);
    const auto cinComp = rootSimById.at(topCin);
    const auto sumComp = rootSimById.at(topSum);
    const auto coutComp = rootSimById.at(topCout);

    const auto rootAOut = firstRealSlot(rootState, aComp->getOutputSlots());
    const auto rootBOut = firstRealSlot(rootState, bComp->getOutputSlots());
    const auto rootCinOut = firstRealSlot(rootState, cinComp->getOutputSlots());
    const auto rootSumIn = firstRealSlot(rootState, sumComp->getInputSlots());
    const auto rootCoutIn = firstRealSlot(rootState, coutComp->getInputSlots());

    const auto topAIn = findNamedSlot(rootState, topModule->getInputSlots(), "a");
    const auto topBIn = findNamedSlot(rootState, topModule->getInputSlots(), "b");
    const auto topCinIn = findNamedSlot(rootState, topModule->getInputSlots(), "cin");
    const auto topSumOut = findNamedSlot(rootState, topModule->getOutputSlots(), "sum");
    const auto topCoutOut = findNamedSlot(rootState, topModule->getOutputSlots(), "cout");

    ASSERT_NE(rootAOut, UUID::null);
    ASSERT_NE(rootBOut, UUID::null);
    ASSERT_NE(rootCinOut, UUID::null);
    ASSERT_NE(rootSumIn, UUID::null);
    ASSERT_NE(rootCoutIn, UUID::null);
    ASSERT_NE(topAIn, UUID::null);
    ASSERT_NE(topBIn, UUID::null);
    ASSERT_NE(topCinIn, UUID::null);
    ASSERT_NE(topSumOut, UUID::null);
    ASSERT_NE(topCoutOut, UUID::null);

    EXPECT_TRUE(hasSceneConnection(rootState, rootAOut, topAIn));
    EXPECT_TRUE(hasSceneConnection(rootState, rootBOut, topBIn));
    EXPECT_TRUE(hasSceneConnection(rootState, rootCinOut, topCinIn));
    EXPECT_TRUE(hasSceneConnection(rootState, topSumOut, rootSumIn));
    EXPECT_TRUE(hasSceneConnection(rootState, topCoutOut, rootCoutIn));

    const auto h1AIn = findNamedSlot(topModuleState, childModules.at("h1")->getInputSlots(), "a");
    const auto h1BIn = findNamedSlot(topModuleState, childModules.at("h1")->getInputSlots(), "b");
    const auto h2BIn = findNamedSlot(topModuleState, childModules.at("h2")->getInputSlots(), "b");
    const auto h2SOut = findNamedSlot(topModuleState, childModules.at("h2")->getOutputSlots(), "s");
    const auto moduleAOut = findNamedSlot(topModuleState, moduleInputSceneComp->getOutputSlots(), "a");
    const auto moduleBOut = findNamedSlot(topModuleState, moduleInputSceneComp->getOutputSlots(), "b");
    const auto moduleCinOut = findNamedSlot(topModuleState, moduleInputSceneComp->getOutputSlots(), "cin");
    const auto moduleSumIn = findNamedSlot(topModuleState, moduleOutputSceneComp->getInputSlots(), "sum");
    const auto moduleCoutIn = findNamedSlot(topModuleState, moduleOutputSceneComp->getInputSlots(), "cout");

    ASSERT_NE(h1AIn, UUID::null);
    ASSERT_NE(h1BIn, UUID::null);
    ASSERT_NE(h2BIn, UUID::null);
    ASSERT_NE(h2SOut, UUID::null);
    ASSERT_NE(moduleAOut, UUID::null);
    ASSERT_NE(moduleBOut, UUID::null);
    ASSERT_NE(moduleCinOut, UUID::null);
    ASSERT_NE(moduleSumIn, UUID::null);
    ASSERT_NE(moduleCoutIn, UUID::null);

    EXPECT_TRUE(hasSceneConnection(topModuleState, moduleAOut, h1AIn));
    EXPECT_TRUE(hasSceneConnection(topModuleState, moduleBOut, h1BIn));
    EXPECT_TRUE(hasSceneConnection(topModuleState, moduleCinOut, h2BIn));
    EXPECT_TRUE(hasSceneConnection(topModuleState, h2SOut, moduleSumIn));

    bool hasDriverForModuleCout = false;
    for (const auto &[uuid, component] : topModuleState.getAllComponents()) {
        (void)uuid;
        if (component->getType() != Bess::Canvas::SceneComponentType::connection) {
            continue;
        }

        const auto conn = std::dynamic_pointer_cast<Bess::Canvas::ConnectionSceneComponent>(component);
        if (conn && conn->getEndSlot() == moduleCoutIn) {
            hasDriverForModuleCout = true;
            break;
        }
    }
    EXPECT_TRUE(hasDriverForModuleCout);

    std::filesystem::remove(verilogPath);
}

TEST_F(VerilogImportTest, ComputesScalesBeforeApplyingHierarchicalLayout) {
    const std::string topModuleName = "very_long_named_top_module_for_layout_check";
    const std::string longInputName = "very_long_input_name_for_layout_spacing_validation_signal";
    const std::string longOutputName = "very_long_output_name_for_layout_spacing_validation_signal";

    const auto verilogPath = writeTempVerilogFile(
        "bess_layout_scale_precompute_test.v",
        R"verilog(
module child_mod(in_sig,out_sig);
  input in_sig;
  output out_sig;
  buf b1(out_sig,in_sig);
endmodule

module very_long_named_top_module_for_layout_check(
    input very_long_input_name_for_layout_spacing_validation_signal,
    output very_long_output_name_for_layout_spacing_validation_signal
);
    child_mod u_child(
        .in_sig(very_long_input_name_for_layout_spacing_validation_signal),
        .out_sig(very_long_output_name_for_layout_spacing_validation_signal)
    );
endmodule
)verilog");

    const auto result = importVerilogFileIntoSimulationEngine(
        verilogPath,
        *engine,
        YosysRunnerConfig{
            .executablePath = "yosys",
            .topModuleName = topModuleName,
        });

    auto scene = Bess::Pages::MainPage::getInstance()->getState().getSceneDriver().getActiveScene();
    ASSERT_NE(scene, nullptr);
    scene->clear();
    Bess::Pages::populateSceneFromVerilogImportResult(result, *engine, *scene);

    std::shared_ptr<Bess::Canvas::SimulationSceneComponent> rootInput;
    std::shared_ptr<Bess::Canvas::SimulationSceneComponent> rootOutput;
    std::shared_ptr<Bess::Canvas::ModuleSceneComponent> topModule;

    for (const auto &[uuid, component] : scene->getState().getAllComponents()) {
        (void)uuid;
        if (component->getParentComponent() != UUID::null) {
            continue;
        }

        if (component->getType() == Bess::Canvas::SceneComponentType::module) {
            const auto moduleComponent =
                std::dynamic_pointer_cast<Bess::Canvas::ModuleSceneComponent>(component);
            if (moduleComponent && moduleComponent->getName() == topModuleName) {
                topModule = moduleComponent;
            }
            continue;
        }

        if (component->getType() != Bess::Canvas::SceneComponentType::simulation) {
            continue;
        }

        const auto simComponent =
            std::dynamic_pointer_cast<Bess::Canvas::SimulationSceneComponent>(component);
        if (!simComponent) {
            continue;
        }

        if (simComponent->getName() == longInputName) {
            rootInput = simComponent;
        } else if (simComponent->getName() == longOutputName) {
            rootOutput = simComponent;
        }
    }

    ASSERT_NE(rootInput, nullptr);
    ASSERT_NE(rootOutput, nullptr);
    ASSERT_NE(topModule, nullptr);

    // Long IO names should force wider components if scales were computed before layout.
    EXPECT_GT(rootInput->getTransform().scale.x, 100.f);
    EXPECT_GT(rootOutput->getTransform().scale.x, 100.f);

    std::filesystem::remove(verilogPath);
}

TEST_F(VerilogImportTest, KeepsVectorTopOutputSlotMappingWhenDriverIsSharedConstant) {
    const auto verilogPath = writeTempVerilogFile(
        buildUniqueTempVerilogFileName("bess_shared_const_output_mapping_test"),
        R"verilog(
module cpu(
    output [15:0] address
);
    assign address = 16'h0000;
endmodule
)verilog");

    const auto result = importVerilogFileIntoSimulationEngine(
        verilogPath,
        *engine,
        YosysRunnerConfig{
            .executablePath = "yosys",
            .topModuleName = std::string("cpu"),
        });

    ASSERT_TRUE(result.topOutputComponents.contains("address"));
    const auto addressOut = result.topOutputComponents.at("address");

    auto &sceneDriver = Bess::Pages::MainPage::getInstance()->getState().getSceneDriver();
    auto rootScene = sceneDriver.getActiveScene();
    ASSERT_NE(rootScene, nullptr);
    rootScene->clear();
    Bess::Pages::populateSceneFromVerilogImportResult(result, *engine, *rootScene);

    std::shared_ptr<Bess::Canvas::ModuleSceneComponent> topModule;
    for (const auto &[uuid, component] : rootScene->getState().getAllComponents()) {
        (void)uuid;
        if (component->getParentComponent() != UUID::null) {
            continue;
        }
        if (component->getType() != Bess::Canvas::SceneComponentType::module) {
            continue;
        }

        const auto moduleComp = std::dynamic_pointer_cast<Bess::Canvas::ModuleSceneComponent>(component);
        if (moduleComp && moduleComp->getName() == "cpu") {
            topModule = moduleComp;
            break;
        }
    }

    ASSERT_NE(topModule, nullptr);
    const auto wrapperId = topModule->getSimEngineId();

    const auto addressConnections = engine->getConnections(addressOut);
    ASSERT_EQ(addressConnections.inputs.size(), 16u);

    for (int dstSlot = 0; dstSlot < 16; ++dstSlot) {
        bool foundExpectedSource = false;
        for (const auto &[srcId, srcSlot] : addressConnections.inputs[dstSlot]) {
            if (srcId == wrapperId && srcSlot == dstSlot) {
                foundExpectedSource = true;
                break;
            }
        }

        EXPECT_TRUE(foundExpectedSource)
            << "Expected wrapper slot " << dstSlot
            << " to drive address output slot " << dstSlot;
    }

    std::filesystem::remove(verilogPath);
}

TEST_F(VerilogImportTest, ImportsSingleTopFileWithIncludeFromSameDirectory) {
    const auto helperFileName = buildUniqueTempVerilogFileName("bess_include_helper_test");
    const auto topFileName = buildUniqueTempVerilogFileName("bess_include_top_test");

    const auto helperPath = writeTempVerilogFile(
        helperFileName,
        R"verilog(
module include_helper(input a, input b, output y);
    assign y = a & b;
endmodule
)verilog");

    const auto topPath = writeTempVerilogFile(
        topFileName,
        std::format(R"verilog(
`include "{}"

module include_top(input a, input b, output y);
    include_helper u0(.a(a), .b(b), .y(y));
endmodule
)verilog",
                    helperFileName));

    const auto result = importVerilogFileIntoSimulationEngine(
        topPath,
        *engine,
        YosysRunnerConfig{
            .executablePath = "yosys",
            .topModuleName = std::string("include_top"),
        });

    ASSERT_EQ(result.topModuleName, "include_top");
    ASSERT_TRUE(result.topInputComponents.contains("a"));
    ASSERT_TRUE(result.topInputComponents.contains("b"));
    ASSERT_TRUE(result.topOutputComponents.contains("y"));

    const auto a = result.topInputComponents.at("a");
    const auto b = result.topInputComponents.at("b");
    const auto y = result.topOutputComponents.at("y");

    engine->setOutputSlotState(a, 0, LogicState::high);
    engine->setOutputSlotState(b, 0, LogicState::high);
    ASSERT_TRUE(waitUntil([&] {
        return engine->getDigitalSlotState(y, SlotType::digitalInput, 0).state == LogicState::high;
    })) << "Include-based helper module did not propagate expected output";

    engine->setOutputSlotState(b, 0, LogicState::low);
    ASSERT_TRUE(waitUntil([&] {
        return engine->getDigitalSlotState(y, SlotType::digitalInput, 0).state == LogicState::low;
    })) << "Include-based helper module did not update output after input change";

    std::filesystem::remove(topPath);
    std::filesystem::remove(helperPath);
}

TEST_F(VerilogImportTest, ImportsDesignFromExplicitMultipleVerilogSourceFiles) {
    const auto childPath = writeTempVerilogFile(
        buildUniqueTempVerilogFileName("bess_multifile_child_test"),
        R"verilog(
module child_and(input a, input b, output y);
    assign y = a & b;
endmodule
)verilog");

    const auto topPath = writeTempVerilogFile(
        buildUniqueTempVerilogFileName("bess_multifile_top_test"),
        R"verilog(
module multi_top(input a, input b, output y);
    child_and u0(.a(a), .b(b), .y(y));
endmodule
)verilog");

    const auto result = importVerilogFilesIntoSimulationEngine(
        std::vector<std::filesystem::path>{topPath, childPath},
        *engine,
        YosysRunnerConfig{
            .executablePath = "yosys",
            .topModuleName = std::string("multi_top"),
        });

    ASSERT_EQ(result.topModuleName, "multi_top");
    ASSERT_TRUE(result.topInputComponents.contains("a"));
    ASSERT_TRUE(result.topInputComponents.contains("b"));
    ASSERT_TRUE(result.topOutputComponents.contains("y"));

    const auto a = result.topInputComponents.at("a");
    const auto b = result.topInputComponents.at("b");
    const auto y = result.topOutputComponents.at("y");

    engine->setOutputSlotState(a, 0, LogicState::high);
    engine->setOutputSlotState(b, 0, LogicState::high);
    ASSERT_TRUE(waitUntil([&] {
        return engine->getDigitalSlotState(y, SlotType::digitalInput, 0).state == LogicState::high;
    })) << "Multi-file design output did not resolve high for 1 & 1";

    engine->setOutputSlotState(a, 0, LogicState::high);
    engine->setOutputSlotState(b, 0, LogicState::low);
    ASSERT_TRUE(waitUntil([&] {
        return engine->getDigitalSlotState(y, SlotType::digitalInput, 0).state == LogicState::low;
    })) << "Multi-file design output did not resolve low for 1 & 0";

    std::filesystem::remove(topPath);
    std::filesystem::remove(childPath);
}

TEST_F(VerilogImportTest, ImportsTriStateDesignWithTopLevelInoutPort) {
    const auto verilogPath = writeTempVerilogFile(
        buildUniqueTempVerilogFileName("bess_tristate_inout_test"),
        R"verilog(
module tri_top(
    input en,
    input d,
    inout bus,
    output q
);
    assign bus = en ? d : 1'bz;
    assign q = bus;
endmodule
)verilog");

    const auto yosysJson = runYosysForJson(
        verilogPath,
        YosysRunnerConfig{
            .executablePath = "yosys",
            .topModuleName = std::string("tri_top"),
        });

    ASSERT_TRUE(yosysJson.isMember("modules"));
    ASSERT_TRUE(yosysJson["modules"].isMember("tri_top"));

    const auto &ports = yosysJson["modules"]["tri_top"]["ports"];
    ASSERT_TRUE(ports.isObject());
    ASSERT_TRUE(ports.isMember("bus"));
    ASSERT_EQ(ports["bus"]["direction"].asString(), "inout");

    const auto result = importVerilogFileIntoSimulationEngine(
        verilogPath,
        *engine,
        YosysRunnerConfig{
            .executablePath = "yosys",
            .topModuleName = std::string("tri_top"),
        });

    ASSERT_EQ(result.topModuleName, "tri_top");
    ASSERT_TRUE(result.topInputComponents.contains("en"));
    ASSERT_TRUE(result.topInputComponents.contains("d"));
    ASSERT_TRUE(result.topInputComponents.contains("bus"));
    ASSERT_TRUE(result.topOutputComponents.contains("bus"));
    ASSERT_TRUE(result.topOutputComponents.contains("q"));

    std::filesystem::remove(verilogPath);
}

TEST_F(VerilogImportTest, ImportsCoarseAddCellFromYosysJson) {
    Json::Value root(Json::objectValue);
    root["modules"] = Json::Value(Json::objectValue);

    auto &top = root["modules"]["top"];
    top["attributes"]["top"] = "1";

    top["ports"]["a"]["direction"] = "input";
    top["ports"]["a"]["bits"].append(1);
    top["ports"]["a"]["bits"].append(2);
    top["ports"]["a"]["bits"].append(3);
    top["ports"]["a"]["bits"].append(4);

    top["ports"]["b"]["direction"] = "input";
    top["ports"]["b"]["bits"].append(5);
    top["ports"]["b"]["bits"].append(6);
    top["ports"]["b"]["bits"].append(7);
    top["ports"]["b"]["bits"].append(8);

    top["ports"]["y"]["direction"] = "output";
    top["ports"]["y"]["bits"].append(9);
    top["ports"]["y"]["bits"].append(10);
    top["ports"]["y"]["bits"].append(11);
    top["ports"]["y"]["bits"].append(12);

    auto &add = top["cells"]["u_add"];
    add["type"] = "$add";
    add["connections"]["A"].append(1);
    add["connections"]["A"].append(2);
    add["connections"]["A"].append(3);
    add["connections"]["A"].append(4);
    add["connections"]["B"].append(5);
    add["connections"]["B"].append(6);
    add["connections"]["B"].append(7);
    add["connections"]["B"].append(8);
    add["connections"]["Y"].append(9);
    add["connections"]["Y"].append(10);
    add["connections"]["Y"].append(11);
    add["connections"]["Y"].append(12);
    add["parameters"]["A_SIGNED"] = "0";
    add["parameters"]["B_SIGNED"] = "0";

    const auto design = parseDesignFromYosysJson(root);
    const auto result = importDesignIntoSimulationEngine(design, *engine);

    const auto a = result.topInputComponents.at("a");
    const auto b = result.topInputComponents.at("b");
    const auto y = result.topOutputComponents.at("y");

    auto writeBus = [&](const UUID &componentId, uint32_t value, size_t width) {
        for (size_t i = 0; i < width; ++i) {
            engine->setOutputSlotState(componentId,
                                       static_cast<int>(i),
                                       ((value >> i) & 1U) ? LogicState::high : LogicState::low);
        }
    };

    auto readBus = [&](const UUID &componentId, size_t width) {
        uint32_t value = 0;
        for (size_t i = 0; i < width; ++i) {
            if (engine->getDigitalSlotState(componentId, SlotType::digitalInput, static_cast<int>(i)).state ==
                LogicState::high) {
                value |= (1U << i);
            }
        }
        return value;
    };

    writeBus(a, 3, 4);
    writeBus(b, 5, 4);
    ASSERT_TRUE(waitUntil([&] { return readBus(y, 4) == 8; }));

    writeBus(a, 9, 4);
    writeBus(b, 8, 4);
    ASSERT_TRUE(waitUntil([&] { return readBus(y, 4) == 1; }));
}

TEST_F(VerilogImportTest, ImportsComparatorAndShiftCellsFromYosysJson) {
    Json::Value root(Json::objectValue);
    root["modules"] = Json::Value(Json::objectValue);

    auto &top = root["modules"]["top"];
    top["attributes"]["top"] = "1";

    top["ports"]["a"]["direction"] = "input";
    top["ports"]["a"]["bits"].append(1);
    top["ports"]["a"]["bits"].append(2);
    top["ports"]["a"]["bits"].append(3);
    top["ports"]["a"]["bits"].append(4);

    top["ports"]["b"]["direction"] = "input";
    top["ports"]["b"]["bits"].append(5);
    top["ports"]["b"]["bits"].append(6);
    top["ports"]["b"]["bits"].append(7);
    top["ports"]["b"]["bits"].append(8);

    top["ports"]["s"]["direction"] = "input";
    top["ports"]["s"]["bits"].append(9);
    top["ports"]["s"]["bits"].append(10);

    top["ports"]["lt"]["direction"] = "output";
    top["ports"]["lt"]["bits"].append(11);

    top["ports"]["sh"]["direction"] = "output";
    top["ports"]["sh"]["bits"].append(12);
    top["ports"]["sh"]["bits"].append(13);
    top["ports"]["sh"]["bits"].append(14);
    top["ports"]["sh"]["bits"].append(15);

    auto &lt = top["cells"]["u_lt"];
    lt["type"] = "$lt";
    lt["connections"]["A"].append(1);
    lt["connections"]["A"].append(2);
    lt["connections"]["A"].append(3);
    lt["connections"]["A"].append(4);
    lt["connections"]["B"].append(5);
    lt["connections"]["B"].append(6);
    lt["connections"]["B"].append(7);
    lt["connections"]["B"].append(8);
    lt["connections"]["Y"].append(11);
    lt["parameters"]["A_SIGNED"] = "0";
    lt["parameters"]["B_SIGNED"] = "0";

    auto &shl = top["cells"]["u_shl"];
    shl["type"] = "$shl";
    shl["connections"]["A"].append(1);
    shl["connections"]["A"].append(2);
    shl["connections"]["A"].append(3);
    shl["connections"]["A"].append(4);
    shl["connections"]["B"].append(9);
    shl["connections"]["B"].append(10);
    shl["connections"]["Y"].append(12);
    shl["connections"]["Y"].append(13);
    shl["connections"]["Y"].append(14);
    shl["connections"]["Y"].append(15);
    shl["parameters"]["A_SIGNED"] = "0";

    const auto design = parseDesignFromYosysJson(root);
    const auto result = importDesignIntoSimulationEngine(design, *engine);

    const auto a = result.topInputComponents.at("a");
    const auto b = result.topInputComponents.at("b");
    const auto s = result.topInputComponents.at("s");
    const auto ltOut = result.topOutputComponents.at("lt");
    const auto shOut = result.topOutputComponents.at("sh");

    auto writeBus = [&](const UUID &componentId, uint32_t value, size_t width) {
        for (size_t i = 0; i < width; ++i) {
            engine->setOutputSlotState(componentId,
                                       static_cast<int>(i),
                                       ((value >> i) & 1U) ? LogicState::high : LogicState::low);
        }
    };

    auto readBus = [&](const UUID &componentId, size_t width) {
        uint32_t value = 0;
        for (size_t i = 0; i < width; ++i) {
            if (engine->getDigitalSlotState(componentId, SlotType::digitalInput, static_cast<int>(i)).state ==
                LogicState::high) {
                value |= (1U << i);
            }
        }
        return value;
    };

    writeBus(a, 3, 4);
    writeBus(b, 5, 4);
    writeBus(s, 1, 2);
    ASSERT_TRUE(waitUntil([&] {
        return engine->getDigitalSlotState(ltOut, SlotType::digitalInput, 0).state == LogicState::high &&
               readBus(shOut, 4) == 6;
    }));

    writeBus(s, 2, 2);
    ASSERT_TRUE(waitUntil([&] { return readBus(shOut, 4) == 12; }));
}

TEST_F(VerilogImportTest, ImportsDlatchCellFromYosysJson) {
    Json::Value root(Json::objectValue);
    root["modules"] = Json::Value(Json::objectValue);

    auto &top = root["modules"]["top"];
    top["attributes"]["top"] = "1";
    top["ports"]["d"]["direction"] = "input";
    top["ports"]["d"]["bits"].append(1);
    top["ports"]["en"]["direction"] = "input";
    top["ports"]["en"]["bits"].append(2);
    top["ports"]["q"]["direction"] = "output";
    top["ports"]["q"]["bits"].append(3);

    auto &latch = top["cells"]["u_latch"];
    latch["type"] = "$dlatch";
    latch["connections"]["D"].append(1);
    latch["connections"]["EN"].append(2);
    latch["connections"]["Q"].append(3);
    latch["parameters"]["EN_POLARITY"] = "1";

    const auto design = parseDesignFromYosysJson(root);
    const auto result = importDesignIntoSimulationEngine(design, *engine);

    const auto d = result.topInputComponents.at("d");
    const auto en = result.topInputComponents.at("en");
    const auto q = result.topOutputComponents.at("q");

    engine->setOutputSlotState(en, 0, LogicState::high);
    engine->setOutputSlotState(d, 0, LogicState::high);
    ASSERT_TRUE(waitUntil([&] {
        return engine->getDigitalSlotState(q, SlotType::digitalInput, 0).state == LogicState::high;
    }));

    engine->setOutputSlotState(en, 0, LogicState::low);
    engine->setOutputSlotState(d, 0, LogicState::low);
    ASSERT_TRUE(waitUntil([&] {
        return engine->getDigitalSlotState(q, SlotType::digitalInput, 0).state == LogicState::high;
    })) << "Latch did not hold value while disabled";

    engine->setOutputSlotState(en, 0, LogicState::high);
    ASSERT_TRUE(waitUntil([&] {
        return engine->getDigitalSlotState(q, SlotType::digitalInput, 0).state == LogicState::low;
    }));
}

TEST_F(VerilogImportTest, ImportsPmuxCellFromYosysJson) {
    Json::Value root(Json::objectValue);
    root["modules"] = Json::Value(Json::objectValue);

    auto &top = root["modules"]["top"];
    top["attributes"]["top"] = "1";

    top["ports"]["a"]["direction"] = "input";
    top["ports"]["a"]["bits"].append(1);
    top["ports"]["a"]["bits"].append(2);
    top["ports"]["b"]["direction"] = "input";
    top["ports"]["b"]["bits"].append(3);
    top["ports"]["b"]["bits"].append(4);
    top["ports"]["c"]["direction"] = "input";
    top["ports"]["c"]["bits"].append(5);
    top["ports"]["c"]["bits"].append(6);
    top["ports"]["s"]["direction"] = "input";
    top["ports"]["s"]["bits"].append(7);
    top["ports"]["s"]["bits"].append(8);
    top["ports"]["y"]["direction"] = "output";
    top["ports"]["y"]["bits"].append(9);
    top["ports"]["y"]["bits"].append(10);

    auto &pmux = top["cells"]["u_pmux"];
    pmux["type"] = "$pmux";
    pmux["connections"]["A"].append(1);
    pmux["connections"]["A"].append(2);
    pmux["connections"]["B"].append(3);
    pmux["connections"]["B"].append(4);
    pmux["connections"]["B"].append(5);
    pmux["connections"]["B"].append(6);
    pmux["connections"]["S"].append(7);
    pmux["connections"]["S"].append(8);
    pmux["connections"]["Y"].append(9);
    pmux["connections"]["Y"].append(10);

    const auto design = parseDesignFromYosysJson(root);
    const auto result = importDesignIntoSimulationEngine(design, *engine);

    const auto a = result.topInputComponents.at("a");
    const auto b = result.topInputComponents.at("b");
    const auto c = result.topInputComponents.at("c");
    const auto s = result.topInputComponents.at("s");
    const auto y = result.topOutputComponents.at("y");

    auto writeBus = [&](const UUID &componentId, uint32_t value, size_t width) {
        for (size_t i = 0; i < width; ++i) {
            engine->setOutputSlotState(componentId,
                                       static_cast<int>(i),
                                       ((value >> i) & 1U) ? LogicState::high : LogicState::low);
        }
    };

    auto readBus = [&](const UUID &componentId, size_t width) {
        uint32_t value = 0;
        for (size_t i = 0; i < width; ++i) {
            if (engine->getDigitalSlotState(componentId, SlotType::digitalInput, static_cast<int>(i)).state ==
                LogicState::high) {
                value |= (1U << i);
            }
        }
        return value;
    };

    writeBus(a, 1, 2);
    writeBus(b, 2, 2);
    writeBus(c, 3, 2);

    writeBus(s, 0, 2);
    ASSERT_TRUE(waitUntil([&] { return readBus(y, 2) == 1; }));

    writeBus(s, 1, 2);
    ASSERT_TRUE(waitUntil([&] { return readBus(y, 2) == 2; }));

    writeBus(s, 2, 2);
    ASSERT_TRUE(waitUntil([&] { return readBus(y, 2) == 3; }));
}

TEST_F(VerilogImportTest, ImportsMemoryReadWriteCellsFromYosysJson) {
    Json::Value root(Json::objectValue);
    root["modules"] = Json::Value(Json::objectValue);

    auto &top = root["modules"]["top"];
    top["attributes"]["top"] = "1";

    top["ports"]["clk"]["direction"] = "input";
    top["ports"]["clk"]["bits"].append(1);
    top["ports"]["we"]["direction"] = "input";
    top["ports"]["we"]["bits"].append(2);
    top["ports"]["addr"]["direction"] = "input";
    top["ports"]["addr"]["bits"].append(3);
    top["ports"]["din"]["direction"] = "input";
    top["ports"]["din"]["bits"].append(4);
    top["ports"]["din"]["bits"].append(5);
    top["ports"]["din"]["bits"].append(6);
    top["ports"]["din"]["bits"].append(7);
    top["ports"]["dout"]["direction"] = "output";
    top["ports"]["dout"]["bits"].append(8);
    top["ports"]["dout"]["bits"].append(9);
    top["ports"]["dout"]["bits"].append(10);
    top["ports"]["dout"]["bits"].append(11);

    auto &memwr = top["cells"]["u_memwr"];
    memwr["type"] = "$memwr_v2";
    memwr["connections"]["ADDR"].append(3);
    memwr["connections"]["CLK"].append(1);
    memwr["connections"]["DATA"].append(4);
    memwr["connections"]["DATA"].append(5);
    memwr["connections"]["DATA"].append(6);
    memwr["connections"]["DATA"].append(7);
    memwr["connections"]["EN"].append(2);
    memwr["connections"]["EN"].append(2);
    memwr["connections"]["EN"].append(2);
    memwr["connections"]["EN"].append(2);
    memwr["parameters"]["ABITS"] = "1";
    memwr["parameters"]["CLK_ENABLE"] = "1";
    memwr["parameters"]["CLK_POLARITY"] = "1";
    memwr["parameters"]["MEMID"] = "\\mem0";
    memwr["parameters"]["WIDTH"] = "4";
    memwr["parameters"]["PORTID"] = "0";
    memwr["parameters"]["PRIORITY_MASK"] = "";

    auto &memrd = top["cells"]["u_memrd"];
    memrd["type"] = "$memrd";
    memrd["connections"]["ADDR"].append(3);
    memrd["connections"]["CLK"].append("x");
    memrd["connections"]["DATA"].append(8);
    memrd["connections"]["DATA"].append(9);
    memrd["connections"]["DATA"].append(10);
    memrd["connections"]["DATA"].append(11);
    memrd["connections"]["EN"].append("x");
    memrd["parameters"]["ABITS"] = "1";
    memrd["parameters"]["CLK_ENABLE"] = "0";
    memrd["parameters"]["CLK_POLARITY"] = "0";
    memrd["parameters"]["MEMID"] = "\\mem0";
    memrd["parameters"]["TRANSPARENT"] = "0";
    memrd["parameters"]["WIDTH"] = "4";

    const auto design = parseDesignFromYosysJson(root);
    const auto result = importDesignIntoSimulationEngine(design, *engine);

    const auto clk = result.topInputComponents.at("clk");
    const auto we = result.topInputComponents.at("we");
    const auto addr = result.topInputComponents.at("addr");
    const auto din = result.topInputComponents.at("din");
    const auto dout = result.topOutputComponents.at("dout");

    auto writeBus = [&](const UUID &componentId, uint32_t value, size_t width) {
        for (size_t i = 0; i < width; ++i) {
            engine->setOutputSlotState(componentId,
                                       static_cast<int>(i),
                                       ((value >> i) & 1U) ? LogicState::high : LogicState::low);
        }
    };

    auto readBus = [&](const UUID &componentId, size_t width) {
        uint32_t value = 0;
        for (size_t i = 0; i < width; ++i) {
            if (engine->getDigitalSlotState(componentId, SlotType::digitalInput, static_cast<int>(i)).state ==
                LogicState::high) {
                value |= (1U << i);
            }
        }
        return value;
    };

    engine->setOutputSlotState(clk, 0, LogicState::low);
    engine->setOutputSlotState(we, 0, LogicState::low);

    auto pulseClock = [&]() {
        engine->setOutputSlotState(clk, 0, LogicState::high);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        engine->setOutputSlotState(clk, 0, LogicState::low);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    };

    writeBus(addr, 0, 1);
    writeBus(din, 0xA, 4);
    engine->setOutputSlotState(we, 0, LogicState::high);
    pulseClock();
    engine->setOutputSlotState(we, 0, LogicState::low);
    ASSERT_TRUE(waitUntil([&] { return readBus(dout, 4) == 0xA; }));

    writeBus(addr, 1, 1);
    writeBus(din, 0x3, 4);
    engine->setOutputSlotState(we, 0, LogicState::high);
    pulseClock();
    engine->setOutputSlotState(we, 0, LogicState::low);
    ASSERT_TRUE(waitUntil([&] { return readBus(dout, 4) == 0x3; }));

    writeBus(addr, 0, 1);
    ASSERT_TRUE(waitUntil([&] { return readBus(dout, 4) == 0xA; }));
}

TEST_F(VerilogImportTest, TemporaryHarvardCpuSmokeExecutesBasicInstructions) {
    const std::filesystem::path cpuDir =
        "Verilog-Harvard-CPU/01. Single-cycle CPU";

    if (!std::filesystem::exists(cpuDir)) {
        GTEST_SKIP() << "Temporary Harvard CPU folder not found: " << cpuDir.string();
    }

    const std::vector<std::filesystem::path> verilogFiles = {
        cpuDir / "cpu.v",
        cpuDir / "ALU.v",
        cpuDir / "Control.v",
        cpuDir / "RF.v",
        cpuDir / "opcodes.v",
    };

    for (const auto &path : verilogFiles) {
        if (!std::filesystem::exists(path)) {
            GTEST_SKIP() << "Missing Harvard CPU source file: " << path.string();
        }
    }

    const auto result = importVerilogFilesIntoSimulationEngine(
        verilogFiles,
        *engine,
        YosysRunnerConfig{
            .executablePath = "yosys",
            .topModuleName = std::string("cpu"),
        });

    ASSERT_TRUE(result.topInputComponents.contains("clk"));
    ASSERT_TRUE(result.topInputComponents.contains("reset_n"));
    ASSERT_TRUE(result.topInputComponents.contains("inputReady"));
    ASSERT_TRUE(result.topInputComponents.contains("data"));
    ASSERT_TRUE(result.topOutputComponents.contains("readM"));
    ASSERT_TRUE(result.topOutputComponents.contains("address"));
    ASSERT_TRUE(result.topOutputComponents.contains("num_inst"));
    ASSERT_TRUE(result.topOutputComponents.contains("output_port"));

    const auto clk = result.topInputComponents.at("clk");
    const auto resetN = result.topInputComponents.at("reset_n");
    const auto inputReady = result.topInputComponents.at("inputReady");
    const auto dataIn = result.topInputComponents.at("data");

    const auto readM = result.topOutputComponents.at("readM");
    const auto address = result.topOutputComponents.at("address");
    const auto numInst = result.topOutputComponents.at("num_inst");
    const auto outputPort = result.topOutputComponents.at("output_port");

    auto writeBus = [&](const UUID &componentId, uint16_t value, size_t width) {
        for (size_t i = 0; i < width; ++i) {
            const auto bit = ((value >> i) & 1U) ? LogicState::high : LogicState::low;
            engine->setOutputSlotState(componentId, static_cast<int>(i), bit);
        }
    };

    auto readBus = [&](const UUID &componentId, size_t width) {
        uint16_t value = 0;
        for (size_t i = 0; i < width; ++i) {
            if (engine->getDigitalSlotState(componentId, SlotType::digitalInput, static_cast<int>(i)).state ==
                LogicState::high) {
                value |= static_cast<uint16_t>(1U << i);
            }
        }
        return value;
    };

    auto readBit = [&](const UUID &componentId) {
        return engine->getDigitalSlotState(componentId, SlotType::digitalInput, 0).state;
    };

    auto pulseInputReady = [&]() {
        engine->setOutputSlotState(inputReady, 0, LogicState::high);
        ASSERT_TRUE(waitUntil([&] { return readBit(readM) == LogicState::low; }))
            << "CPU did not acknowledge memory response (readM stayed high)";
        engine->setOutputSlotState(inputReady, 0, LogicState::low);
    };

    auto clockTick = [&]() {
        const auto before = readBus(numInst, 16);
        engine->setOutputSlotState(clk, 0, LogicState::high);
        ASSERT_TRUE(waitUntil([&] { return readBus(numInst, 16) != before; }))
            << "CPU did not react to clock rising edge";
        engine->setOutputSlotState(clk, 0, LogicState::low);
    };

    auto fetchInstruction = [&](uint16_t expectedAddress, uint16_t instructionWord) {
        ASSERT_TRUE(waitUntil([&] { return readBit(readM) == LogicState::high; }))
            << "CPU did not request instruction fetch";

        ASSERT_EQ(readBus(address, 16), expectedAddress)
            << "Unexpected instruction fetch address";

        writeBus(dataIn, instructionWord, 16);
        pulseInputReady();
    };

    // Initialize external inputs and reset sequence.
    engine->setOutputSlotState(clk, 0, LogicState::low);
    engine->setOutputSlotState(inputReady, 0, LogicState::low);
    writeBus(dataIn, 0, 16);
    engine->setOutputSlotState(resetN, 0, LogicState::low);
    engine->setOutputSlotState(resetN, 0, LogicState::high);

    // Program:
    //   0x6034: LHI r0, 0x34   -> r0 = 0x3400
    //   0xF01C: WWD r0         -> output_port should expose r0
    fetchInstruction(0x0000, 0x6034);
    clockTick();

    fetchInstruction(0x0001, 0xF01C);
    ASSERT_TRUE(waitUntil([&] { return readBus(outputPort, 16) == 0x3400; }))
        << "WWD did not expose expected register value on output_port";

    clockTick();
    EXPECT_GE(readBus(numInst, 16), static_cast<uint16_t>(3));
}
