#include "application/pages/main_page/verilog_scene_import.h"
#include "bverilog/sim_engine_importer.h"
#include "bverilog/yosys_json_parser.h"
#include "bverilog/yosys_runner.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "scene/scene.h"
#include "simulation_engine.h"
#include "types.h"
#include "gtest/gtest.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <json/value.h>
#include <memory>
#include <thread>

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

    auto resizeOutputs = [](const std::shared_ptr<DigitalComponent> &component, size_t count) {
        while (component->definition->getOutputSlotsInfo().count < count) {
            component->incrementOutputCount(true);
        }
    };

    const auto topA = result.topInputComponents.at("a");
    const auto inputDefinition = engine->getComponentDefinition(topA);
    const auto bridgeInputId = engine->addComponent(inputDefinition);
    const auto bridgeInput = engine->getDigitalComponent(bridgeInputId);
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

    EXPECT_EQ(engine->getDigitalComponent(a)->definition->getOutputSlotsInfo().count, 8);
    EXPECT_EQ(engine->getDigitalComponent(b)->definition->getOutputSlotsInfo().count, 8);
    EXPECT_EQ(engine->getDigitalComponent(aluSel)->definition->getOutputSlotsInfo().count, 4);
    EXPECT_EQ(engine->getDigitalComponent(aluOut)->definition->getInputSlotsInfo().count, 8);
    EXPECT_EQ(engine->getDigitalComponent(carryOut)->definition->getInputSlotsInfo().count, 1);

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

    Bess::Canvas::Scene scene;
    Bess::Pages::populateSceneFromVerilogImportResult(result, *engine, scene);

    bool foundAluOut = false;
    bool foundCarryOut = false;
    const auto &sceneState = scene.getState();

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
