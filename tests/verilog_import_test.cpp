#include "application/pages/main_page/verilog_scene_import.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/scene_components/connection_scene_component.h"
#include "pages/main_page/scene_components/module_scene_component.h"
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
