#include "bverilog/sim_engine_importer.h"
#include "bverilog/yosys_json_parser.h"
#include "bverilog/yosys_runner.h"
#include "gtest/gtest.h"
#include "simulation_engine.h"
#include "types.h"
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
        engine = std::make_unique<SimulationEngine>();
        engine->setSimulationState(SimulationState::running);
        engine->clear();
    }

    void TearDown() override {
        if (engine) {
        engine->setSimulationState(SimulationState::paused);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
            engine->clear();
            engine->destroy();
            engine.reset();
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

    std::unique_ptr<SimulationEngine> engine;
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
    ASSERT_TRUE(result.topInputComponents.contains("in0"));
    ASSERT_TRUE(result.topInputComponents.contains("in1"));
    ASSERT_TRUE(result.topOutputComponents.contains("out0"));

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
