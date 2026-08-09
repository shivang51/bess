#include "bess_core/g_app_context.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/scene.h"
#include "bess_core/scene/scene_event.h"
#include "bess_core/scene/scene_serializer.h"
#include "bess_core/scene/scene_ui/controls/scalar_input_comp.h"
#include "bess_core/scene/scene_ui/controls/text_box_comp.h"
#include "bess_core/scene/scene_ui/controls/toggle_btn_comp.h"
#include "bess_core/scene_driver.h"
#include "component_catalog.h"
#include "dig_module_def.h"
#include "dig_sim_driver.h"
#include "event_dispatcher.h"
#include "math_sim_driver.h"
#include "pages/main_page/project_model.h"
#include "pages/main_page/main_page_state.h"
#include "pages/main_page/module_edit.h"
#include "pages/main_page/scene_components/connection_scene_component.h"
#include "pages/main_page/scene_components/input_scene_component.h"
#include "pages/main_page/scene_components/module_scene_component.h"
#include "pages/main_page/scene_components/monitor_scene_comp.h"
#include "pages/main_page/scene_components/non_sim_scene_component.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "pages/main_page/services/connection_service.h"
#include "pages/main_page/services/copy_paste_service.h"
#include "plugin_manager.h"
#include "project_session/project_session.h"
#include "simulation_engine.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <gtest/gtest.h>
#include <json/reader.h>
#include <memory>
#include <pybind11/embed.h>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace py = pybind11;

namespace {
    using Bess::UUID;
    using Bess::Canvas::ConnectionSceneComponent;
    using Bess::Canvas::Scene;
    using Bess::Canvas::SceneComponent;
    using Bess::Canvas::SimulationSceneComponent;
    using Bess::Canvas::SlotSceneComponent;
    using Bess::SimEngine::Drivers::Digital::DigCompDef;

    std::shared_ptr<DigCompDef> makeDefinition(std::string name,
                                               size_t inputCount,
                                               size_t outputCount,
                                               bool inputsResizable = false,
                                               bool outputsResizable = false,
                                               bool keepIOCountEq = false,
                                               char op = '0') {
        auto definition = std::make_shared<DigCompDef>();
        definition->setName(name);
        definition->setGroupName("Test");
        definition->setKeepIOCountEq(keepIOCountEq);
        Bess::SimEngine::OperatorInfo opInfo;
        opInfo.op = op;
        definition->setOpInfo(opInfo);
        definition->setInputSlotsInfo({Bess::SimEngine::SlotsGroupType::input,
                                       inputsResizable,
                                       inputCount,
                                       {},
                                       {}});
        definition->setOutputSlotsInfo({Bess::SimEngine::SlotsGroupType::output,
                                        outputsResizable,
                                        outputCount,
                                        {},
                                        {}});
        definition->setPropDelay(Bess::TimeNs(1));
        definition->setSimFn(
            [](const std::shared_ptr<
                Bess::SimEngine::Drivers::Digital::DigCompSimData> &data) {
                return data;
            });
        return definition;
    }

    bool containsUuid(const std::vector<UUID> &values, const UUID &id) {
        return std::ranges::find(values, id) != values.end();
    }

    class TestRenderer2D final : public Bess::Core::Renderer::IRenderer2D {
      public:
        std::vector<Bess::Core::Renderer::LineProps> lines;
        std::vector<Bess::Core::Renderer::PathProps> paths;
        std::size_t pathLineCount = 0;

        void init(const Bess::Core::Renderer::Renderer2DCreateInfo &) override {
        }
        void destroy() override {
        }
        void resize(const Bess::Core::Renderer::Renderer2DExtent &) override {
        }
        void
        beginFrame(const Bess::Core::Renderer::Renderer2DFrameInfo &) override {
        }
        void endFrame() override {
        }
        void clear(const Bess::Core::Renderer::Color &) override {
        }
        void saveTargetToFile(const std::string &) override {
        }
        [[nodiscard]] Bess::Core::Renderer::Renderer2DStats
        getStats() const noexcept override {
            return {};
        }
        [[nodiscard]] Bess::Core::Renderer::TextureReadbackResult readTexture(
            const Bess::Core::Renderer::TextureReadbackRegion &) override {
            return {};
        }
        void requestPickingIds(
            const Bess::Core::Renderer::TextureReadbackRegion &) override {
        }
        [[nodiscard]] bool tryGetPickingIds(
            Bess::Core::Renderer::PickingReadbackResult &) override {
            return false;
        }
        [[nodiscard]] bool isPickingReadbackPending() const noexcept override {
            return false;
        }
        void pushScissorRect(
            const Bess::Core::Renderer::RendererScissorRect &) override {
        }
        void popScissorRect() override {
        }
        void clearScissorRects() override {
        }
        void drawQuad(const Bess::Core::Renderer::QuadProps &) override {
        }
        [[nodiscard]] Bess::Core::Renderer::CustomQuadShaderHandle
        createCustomQuadShader(
            const Bess::Core::Renderer::CustomQuadShaderDesc &) override {
            return 1;
        }
        void destroyCustomQuadShader(
            Bess::Core::Renderer::CustomQuadShaderHandle) override {
        }
        void
        drawCustomQuad(const Bess::Core::Renderer::CustomQuadProps &) override {
        }
        void drawCircle(const Bess::Core::Renderer::CircleProps &) override {
        }
        void drawLine(
            const Bess::Core::Renderer::LineProps &props) override {
            lines.push_back(props);
        }
        void drawFont(std::string_view,
                      const Bess::Core::Renderer::FontProps & = {}) override {
        }
        [[nodiscard]] glm::vec2 measureText(
            std::string_view text,
            const Bess::Core::Renderer::FontProps &props = {}) override {
            return getTextRenderSize(text, props);
        }
        [[nodiscard]] float textCenterOffsetY(
            std::string_view,
            const Bess::Core::Renderer::FontProps &props = {}) override {
            return props.fontSize * 0.35f;
        }
        void drawPath(std::span<const Bess::Core::Renderer::PathCommand>,
                      const Bess::Core::Renderer::PathProps & = {}) override {
        }
        void
        beginPath(const Bess::Core::Renderer::PathProps &props = {}) override {
            paths.push_back(props);
        }
        void pathMoveTo(const glm::vec2 &) override {
        }
        void pathLineTo(
            const glm::vec2 &,
            const Bess::Core::Renderer::PathCommandStroke & = {}) override {
            ++pathLineCount;
        }
        void pathQuadTo(
            const glm::vec2 &,
            const glm::vec2 &,
            const Bess::Core::Renderer::PathCommandStroke & = {}) override {
        }
        void pathCubicTo(
            const glm::vec2 &,
            const glm::vec2 &,
            const glm::vec2 &,
            const Bess::Core::Renderer::PathCommandStroke & = {}) override {
        }
        void pathClose(
            const Bess::Core::Renderer::PathCommandStroke & = {}) override {
        }
        void endPath() override {
        }
    };

    struct SimComponentFixture {
        std::shared_ptr<SimulationSceneComponent> comp;
        std::vector<std::shared_ptr<SlotSceneComponent>> inputs;
        std::vector<std::shared_ptr<SlotSceneComponent>> outputs;
        std::vector<std::shared_ptr<SceneComponent>> children;
    };

    SimComponentFixture createSimComponent(
        const std::shared_ptr<Bess::SimEngine::Drivers::CompDef> &definition) {
        const auto created = SimulationSceneComponent::createNew(definition);
        SimComponentFixture fixture;
        if (created.empty()) {
            return fixture;
        }

        fixture.comp =
            std::dynamic_pointer_cast<SimulationSceneComponent>(created[0]);
        for (size_t i = 1; i < created.size(); ++i) {
            fixture.children.push_back(created[i]);
            auto slot =
                std::dynamic_pointer_cast<SlotSceneComponent>(created[i]);
            if (!slot) {
                continue;
            }

            if (slot->isInputSlot()) {
                fixture.inputs.push_back(slot);
            } else {
                fixture.outputs.push_back(slot);
            }
        }
        return fixture;
    }

    std::shared_ptr<Bess::SimEngine::Drivers::Math::MathCompDef>
    makeScalarDefinition(std::string name,
                         size_t inputCount,
                         size_t outputCount) {
        auto definition =
            std::make_shared<Bess::SimEngine::Drivers::Math::MathCompDef>();
        definition->setName(std::move(name));
        definition->setGroupName("Math");
        definition->setInputPortDescriptor({
            .direction = Bess::SimEngine::PortDirection::input,
            .signalKind = Bess::SimEngine::SignalKind::scalar,
            .quantityKind = Bess::SimEngine::QuantityKind::dimensionless,
            .count = inputCount,
        });
        definition->setOutputPortDescriptor({
            .direction = Bess::SimEngine::PortDirection::output,
            .signalKind = Bess::SimEngine::SignalKind::scalar,
            .quantityKind = Bess::SimEngine::QuantityKind::dimensionless,
            .count = outputCount,
        });
        definition->setScalarFn(
            [](Bess::TimeMs, const std::vector<double> &values) {
                return values.empty() ? 0.0 : values.front();
            });
        return definition;
    }

    size_t countScalarInputs(const Bess::Canvas::SceneState &state) {
        size_t count = 0;
        for (const auto &[_, component] : state.getAllComponents()) {
            if (std::dynamic_pointer_cast<Bess::Canvas::UI::ScalarInputComp>(
                    component)) {
                ++count;
            }
        }
        return count;
    }

    size_t countUiComps(const Bess::Canvas::SceneState &state) {
        return std::ranges::count_if(
            state.getAllComponents(), [](const auto &entry) {
                return entry.second && entry.second->getType() ==
                                           Bess::Canvas::SceneComponentType::ui;
            });
    }

    std::shared_ptr<Bess::Canvas::UI::ScalarInputComp>
    firstScalarInput(const Bess::Canvas::SceneState &state) {
        for (const auto &[_, component] : state.getAllComponents()) {
            if (auto input =
                    std::dynamic_pointer_cast<Bess::Canvas::UI::ScalarInputComp>(
                        component)) {
                return input;
            }
        }
        return nullptr;
    }

    void measureSceneUi(Bess::Canvas::SceneState &state) {
        for (auto &[_, node] : state.getUINodeRegistry()->getAllNodes()) {
            if (node.getParentId() == UUID::null) {
                node.measure(*state.getUINodeRegistry(), UUID::null);
            }
        }
    }

    Bess::Canvas::SceneEvent textInputEvent(char32_t codepoint) {
        Bess::Canvas::SceneEvent::Data data;
        data.textInput = {.codepoint = codepoint};
        return {
            .type = Bess::Canvas::SceneEvent::Type::textInput,
            .data = data,
        };
    }
} // namespace

class MainPageConnectionCommandsTest : public testing::Test {
  protected:
    void SetUp() override {
        auto &appCtx = Bess::GAppContext::getInstance();
        if (!appCtx.hasSubSystem<Bess::EventSystem::EventDispatcher>()) {
            appCtx.addSubSystem<Bess::EventSystem::EventDispatcher>()->onInit();
        } else {
            appCtx.getSubSystem<Bess::EventSystem::EventDispatcher>()->clear();
            appCtx.getSubSystem<Bess::EventSystem::EventDispatcher>()
                ->dispatchAll();
        }

        if (!appCtx.hasSubSystem<Bess::ProjectSession>()) {
            session = appCtx.addSubSystem<Bess::ProjectSession>();
            session->addSubSystem<Bess::Svc::SvcConnection>();
            session->onInit();
        } else {
            session = appCtx.getSubSystem<Bess::ProjectSession>();
            session->addSubSystem<Bess::Svc::SvcConnection>();
            if (!session->hasSubSystem<Bess::SceneDriver>()) {
                session->onInit();
            }
        }

        sceneDriver = session->getSubSystem<Bess::SceneDriver>();
        simEngine = session->getSubSystem<Bess::SimEngine::SimulationEngine>();
        connectionService = session->getSubSystem<Bess::Svc::SvcConnection>();

        connectionService->onDestroy();
        connectionService->onInit();

        simEngine->clear();
        simEngine->setSimulationState(Bess::SimEngine::SimulationState::paused);

        sceneDriver->removeScenes();
        scene = std::make_shared<Scene>(false);
        scene->getState().setIsRootScene(true);
        sceneDriver->addScene(scene);
        sceneDriver->setRootSceneId(scene->getSceneId());
        sceneDriver->setActiveScene(scene->getSceneId());

        session->clearHist();
        Bess::Pages::initProjectModel(*session);

        sourceDef = makeDefinition("Source", 0, 1);
        sinkDef = makeDefinition("Sink", 1, 0);
    }

    void TearDown() override {
        if (scene) {
            scene->clear();
            scene.reset();
        }

        if (sceneDriver) {
            sceneDriver->removeScenes();
        }

        if (simEngine) {
            simEngine->clear();
            simEngine->setSimulationState(
                Bess::SimEngine::SimulationState::paused);
        }

        if (connectionService) {
            connectionService->onDestroy();
            connectionService->onInit();
        }

        auto &appCtx = Bess::GAppContext::getInstance();
        if (session) {
            session->onDestroy();
            session.reset();
        }
        if (appCtx.hasSubSystem<Bess::ProjectSession>()) {
            appCtx.removeSubSystem<Bess::ProjectSession>();
        }

        if (appCtx.hasSubSystem<Bess::EventSystem::EventDispatcher>()) {
            appCtx.getSubSystem<Bess::EventSystem::EventDispatcher>()->clear();
        }
    }

    SimComponentFixture addSimComponent(
        const std::shared_ptr<Bess::SimEngine::Drivers::CompDef> &definition) {
        auto fixture = createSimComponent(definition);
        EXPECT_NE(fixture.comp, nullptr);
        if (!fixture.comp) {
            return fixture;
        }

        const auto result = session->addComp(
            fixture.comp, fixture.children, scene->getSceneId());
        EXPECT_TRUE(result) << result.status.msg();
        EXPECT_NE(scene->getState().getComponentByUuid(fixture.comp->getUuid()),
                  nullptr);
        EXPECT_NE(fixture.comp->getSimEngineId(), UUID::null);
        return fixture;
    }

    void expectConnectionRestored(
        const SimComponentFixture &source,
        const SimComponentFixture &sink,
        const std::shared_ptr<ConnectionSceneComponent> &connection) const {
        ASSERT_NE(connection, nullptr);
        ASSERT_FALSE(source.outputs.empty());
        ASSERT_FALSE(sink.inputs.empty());

        EXPECT_NE(scene->getState().getComponentByUuid(source.comp->getUuid()),
                  nullptr);
        EXPECT_NE(scene->getState().getComponentByUuid(
                      source.outputs.front()->getUuid()),
                  nullptr);
        EXPECT_NE(scene->getState().getComponentByUuid(connection->getUuid()),
                  nullptr);
        EXPECT_TRUE(
            containsUuid(source.outputs.front()->getConnectedConnections(),
                         connection->getUuid()));
        EXPECT_TRUE(containsUuid(sink.inputs.front()->getConnectedConnections(),
                                 connection->getUuid()));
        EXPECT_NE(source.comp->getSimEngineId(), UUID::null);
    }

    size_t simSlotCount(const SimComponentFixture &fixture,
                        bool inputSlots) const {
        const auto digComp =
            simEngine
                ->getComponent<Bess::SimEngine::Drivers::Digital::DigSimComp>(
                    fixture.comp->getSimEngineId());
        EXPECT_NE(digComp, nullptr);
        if (!digComp) {
            return 0;
        }

        const auto digDef = digComp->getDefinition<DigCompDef>();
        EXPECT_NE(digDef, nullptr);
        if (!digDef) {
            return 0;
        }

        return inputSlots ? digDef->getInputSlotsInfo().count
                          : digDef->getOutputSlotsInfo().count;
    }

    size_t sceneRealSlotCount(const SimComponentFixture &fixture,
                              bool inputSlots) const {
        const auto &slotIds = inputSlots ? fixture.comp->getInputSlots()
                                         : fixture.comp->getOutputSlots();
        size_t count = 0;
        for (const auto &slotId : slotIds) {
            const auto slot =
                scene->getState().getComponentByUuid<SlotSceneComponent>(
                    slotId);
            if (slot && !slot->isResizeSlot()) {
                ++count;
            }
        }
        return count;
    }

    std::shared_ptr<Bess::ProjectSession> session;
    std::shared_ptr<Bess::SceneDriver> sceneDriver;
    std::shared_ptr<Bess::SimEngine::SimulationEngine> simEngine;
    std::shared_ptr<Bess::Svc::SvcConnection> connectionService;
    std::shared_ptr<Scene> scene;
    std::shared_ptr<DigCompDef> sourceDef;
    std::shared_ptr<DigCompDef> sinkDef;
};

TEST(SimulationSceneComponentSlotDirtyTest, SlotMutatorsMarkUIDirty) {
    auto comp = std::make_shared<SimulationSceneComponent>();

    const UUID inputA;
    comp->setUIDirty(false);
    comp->addInputSlot(inputA, false);
    EXPECT_TRUE(comp->getUIDirty());
    EXPECT_TRUE(containsUuid(comp->getInputSlots(), inputA));

    const UUID inputB;
    comp->setUIDirty(false);
    comp->insertInputSlot(inputB, 0);
    EXPECT_TRUE(comp->getUIDirty());
    ASSERT_FALSE(comp->getInputSlots().empty());
    EXPECT_EQ(comp->getInputSlots().front(), inputB);

    comp->setUIDirty(false);
    EXPECT_TRUE(comp->removeInputSlot(inputA));
    EXPECT_TRUE(comp->getUIDirty());
    EXPECT_FALSE(containsUuid(comp->getInputSlots(), inputA));

    comp->setUIDirty(false);
    EXPECT_FALSE(comp->removeInputSlot(inputA));
    EXPECT_FALSE(comp->getUIDirty());

    const UUID outputA;
    comp->setUIDirty(false);
    comp->addOutputSlot(outputA);
    EXPECT_TRUE(comp->getUIDirty());
    EXPECT_TRUE(containsUuid(comp->getOutputSlots(), outputA));

    const std::vector<UUID> outputSlots{outputA};
    comp->setUIDirty(false);
    comp->setOutputSlots(outputSlots);
    EXPECT_FALSE(comp->getUIDirty());

    const UUID outputB;
    comp->setUIDirty(false);
    comp->setOutputSlots({outputA, outputB});
    EXPECT_TRUE(comp->getUIDirty());
    EXPECT_TRUE(containsUuid(comp->getOutputSlots(), outputB));

    comp->setUIDirty(false);
    EXPECT_TRUE(comp->removeOutputSlot(outputA));
    EXPECT_TRUE(comp->getUIDirty());
    EXPECT_FALSE(containsUuid(comp->getOutputSlots(), outputA));
}

TEST_F(MainPageConnectionCommandsTest,
       AddedComponentUndoIsNotShadowedByInternalUiEvents) {
    Bess::Pages::MainPageState mainPageState;
    mainPageState.init();

    const auto fixture = addSimComponent(makeDefinition("UI component", 1, 1));
    ASSERT_NE(fixture.comp, nullptr);
    const auto renderer = std::make_shared<TestRenderer2D>();
    Bess::SceneUIPrepareCtx prepareCtx{
        .sceneState = &scene->getState(),
        .renderer = renderer,
        .parentNode = nullptr,
        .theme = Bess::Core::Style::BessTheme::defaultTheme(),
    };
    fixture.comp->prepareUI(prepareCtx);

    auto dispatcher = Bess::GAppContext::getInstance()
                          .getSubSystem<Bess::EventSystem::EventDispatcher>();
    dispatcher->dispatchAll();

    ASSERT_EQ(session->view().undoCount, 1u);
    ASSERT_TRUE(session->undo());
    EXPECT_EQ(scene->getState().getComponentByUuid(fixture.comp->getUuid()),
              nullptr);
}

TEST_F(MainPageConnectionCommandsTest, AddConnectionIsUndoableAndRedoable) {
    const auto source = addSimComponent(sourceDef);
    const auto sink = addSimComponent(sinkDef);
    ASSERT_NE(source.comp, nullptr);
    ASSERT_NE(sink.comp, nullptr);
    ASSERT_FALSE(source.outputs.empty());
    ASSERT_FALSE(sink.inputs.empty());

    session->clearHist();
    auto connection = std::make_shared<ConnectionSceneComponent>();
    connection->setStartEndSlots(source.outputs.front()->getUuid(),
                                 sink.inputs.front()->getUuid());

    const auto added = session->addConn(connection, scene->getSceneId());
    ASSERT_TRUE(added) << added.status.msg();
    ASSERT_EQ(session->view().undoCount, 1u);
    expectConnectionRestored(source, sink, connection);

    const auto undone = session->undo();
    ASSERT_TRUE(undone) << undone.status.msg();
    EXPECT_EQ(scene->getState().getComponentByUuid(connection->getUuid()),
              nullptr);
    EXPECT_FALSE(containsUuid(source.outputs.front()->getConnectedConnections(),
                              connection->getUuid()));
    EXPECT_FALSE(containsUuid(sink.inputs.front()->getConnectedConnections(),
                              connection->getUuid()));

    const auto redone = session->redo();
    ASSERT_TRUE(redone) << redone.status.msg();
    expectConnectionRestored(source, sink, connection);
}

TEST_F(MainPageConnectionCommandsTest, SchematicMoveEventIsUndoable) {
    Bess::Pages::MainPageState mainPageState;
    mainPageState.init();

    const auto fixture = addSimComponent(sourceDef);
    ASSERT_NE(fixture.comp, nullptr);
    session->clearHist();

    auto transform = fixture.comp->getSchematicTransform();
    const auto start = transform.position;
    transform.position = {64.f, 32.f, 0.f};
    fixture.comp->setSchematicTransform(transform);

    auto dispatcher = Bess::GAppContext::getInstance()
                          .getSubSystem<Bess::EventSystem::EventDispatcher>();
    dispatcher->queue(Bess::Canvas::Events::EntityMovedEvent{
        .entityUuid = fixture.comp->getUuid(),
        .oldPos = start,
        .newPos = transform.position,
        .state = &scene->getState(),
        .schematic = true,
    });
    dispatcher->dispatchAll();

    ASSERT_EQ(session->view().undoCount, 1u);
    ASSERT_TRUE(session->undo());
    EXPECT_EQ(fixture.comp->getSchematicTransform().position, start);

    ASSERT_TRUE(session->redo());
    EXPECT_EQ(fixture.comp->getSchematicTransform().position,
              transform.position);
}

TEST_F(MainPageConnectionCommandsTest, ComponentDragIsUndoable) {
    Bess::Pages::MainPageState mainPageState;
    mainPageState.init();

    auto comp = std::make_shared<Bess::Canvas::NonSimSceneComponent>();
    ASSERT_TRUE(session->addComp(comp, {}, scene->getSceneId()));
    const auto start = comp->getTransform().position;
    session->clearHist();

    const Bess::Canvas::Events::MouseDraggedEvent begin{
        .mousePos = {0.f, 0.f},
        .delta = {},
        .details = 0,
        .isMultiDrag = false,
        .sceneState = &scene->getState(),
        .isSchematicMode = false,
    };
    comp->onMouseDragged(begin);
    auto moved = begin;
    moved.mousePos = {40.f, 24.f};
    comp->onMouseDragged(moved);
    comp->onMouseDragEnd();

    auto dispatcher = Bess::GAppContext::getInstance()
                          .getSubSystem<Bess::EventSystem::EventDispatcher>();
    dispatcher->dispatchAll();
    ASSERT_EQ(session->view().undoCount, 1u);
    ASSERT_NE(comp->getTransform().position, start);

    ASSERT_TRUE(session->undo());
    EXPECT_EQ(comp->getTransform().position, start);

    ASSERT_TRUE(session->redo());
    EXPECT_NE(comp->getTransform().position, start);
}

TEST_F(MainPageConnectionCommandsTest,
       ModuleCreationMovesComponentUiToModuleScene) {
    const auto fixture = addSimComponent(sourceDef);
    ASSERT_NE(fixture.comp, nullptr);
    ASSERT_FALSE(fixture.outputs.empty());

    const UUID net;
    fixture.comp->setNetId(net);

    const auto renderer = std::make_shared<TestRenderer2D>();
    Bess::SceneUIPrepareCtx sourceCtx{
        .sceneState = &scene->getState(),
        .renderer = renderer,
        .parentNode = nullptr,
        .theme = Bess::Core::Style::BessTheme::defaultTheme(),
    };
    fixture.comp->prepareUI(sourceCtx);
    ASSERT_GT(countUiComps(scene->getState()), 0u);

    session->clearHist();
    const auto made =
        Bess::Edit::makeModule(*session, scene, net, "Extracted module");
    ASSERT_TRUE(made) << made.status.msg();
    ASSERT_NE(made.val, nullptr);

    const auto moduleScene =
        sceneDriver->getSceneWithId(made.val->getSceneId());
    ASSERT_NE(moduleScene, nullptr);
    EXPECT_EQ(scene->getState().getComponentByUuid(fixture.comp->getUuid()),
              nullptr);
    EXPECT_NE(
        moduleScene->getState().getComponentByUuid(fixture.comp->getUuid()),
        nullptr);
    EXPECT_EQ(countUiComps(scene->getState()), 0u);
    EXPECT_TRUE(fixture.comp->getUIDirty());

    Bess::SceneUIPrepareCtx moduleCtx{
        .sceneState = &moduleScene->getState(),
        .renderer = renderer,
        .parentNode = nullptr,
        .theme = Bess::Core::Style::BessTheme::defaultTheme(),
    };
    fixture.comp->prepareUI(moduleCtx);
    EXPECT_GT(countUiComps(moduleScene->getState()), 0u);
    EXPECT_EQ(countUiComps(scene->getState()), 0u);

    ASSERT_TRUE(session->undo());
    EXPECT_NE(scene->getState().getComponentByUuid(fixture.comp->getUuid()),
              nullptr);
    EXPECT_EQ(sceneDriver->getSceneWithId(moduleScene->getSceneId()), nullptr);
    EXPECT_EQ(countUiComps(moduleScene->getState()), 0u);
    EXPECT_TRUE(fixture.comp->getUIDirty());

    ASSERT_TRUE(session->redo());
    EXPECT_EQ(scene->getState().getComponentByUuid(fixture.comp->getUuid()),
              nullptr);
    EXPECT_NE(sceneDriver->getSceneWithId(moduleScene->getSceneId()), nullptr);
    EXPECT_NE(
        moduleScene->getState().getComponentByUuid(fixture.comp->getUuid()),
        nullptr);
}

TEST_F(MainPageConnectionCommandsTest,
       ModuleCreationDoesNotRetainClockUiInSourceScene) {
    auto &pluginManager = Bess::Plugins::PluginManager::getInstance();
    ASSERT_TRUE(pluginManager.loadPluginsFromDirectory("plugins"));

    std::shared_ptr<Bess::SimEngine::Drivers::CompDef> clockDefinition;
    std::shared_ptr<SimulationSceneComponent> clock;
    {
        py::gil_scoped_acquire gil;
        auto clockDefinitionModule = py::module_::import("components.clock");
        auto pythonClockDefinition =
            clockDefinitionModule.attr("ClockDefinition")();
        clockDefinition =
            pythonClockDefinition
                .cast<std::shared_ptr<Bess::SimEngine::Drivers::CompDef>>();

        auto clockSceneModule = py::module_::import("scene.clock_comp");
        auto pythonClock = clockSceneModule.attr("ClockComp")();
        pythonClock.attr("setup")(pythonClockDefinition);
        clock = pythonClock.cast<
            std::shared_ptr<Bess::Canvas::SimulationSceneComponent>>();
        clock->setCompDef(clockDefinition->clone());
    }
    ASSERT_NE(clockDefinition, nullptr);
    ASSERT_NE(clock, nullptr);

    scene->addComponent(clock);
    std::vector<std::shared_ptr<SceneComponent>> slots;
    for (const auto slotId : clock->getOutputSlots()) {
        const auto slot =
            scene->getState().getComponentByUuidSP<SlotSceneComponent>(slotId);
        ASSERT_NE(slot, nullptr);
        scene->getState().attachChild(clock->getUuid(), slotId, false);
        slots.push_back(slot);
    }
    ASSERT_FALSE(slots.empty());
    ASSERT_TRUE(session->trackAdd(clock, slots, scene->getSceneId()));

    const UUID net;
    clock->setNetId(net);
    const auto renderer = std::make_shared<TestRenderer2D>();
    Bess::SceneUIPrepareCtx sourceCtx{
        .sceneState = &scene->getState(),
        .renderer = renderer,
        .parentNode = nullptr,
        .theme = Bess::Core::Style::BessTheme::defaultTheme(),
    };
    clock->prepareUI(sourceCtx);
    ASSERT_EQ(countUiComps(scene->getState()), 12u);

    session->clearHist();
    const auto made =
        Bess::Edit::makeModule(*session, scene, net, "Clock module");
    ASSERT_TRUE(made) << made.status.msg();
    ASSERT_NE(made.val, nullptr);

    const auto moduleScene =
        sceneDriver->getSceneWithId(made.val->getSceneId());
    ASSERT_NE(moduleScene, nullptr);
    EXPECT_EQ(countUiComps(scene->getState()), 0u);
    EXPECT_EQ(scene->getState().getComponentByUuid(clock->getUuid()), nullptr);
    EXPECT_NE(moduleScene->getState().getComponentByUuid(clock->getUuid()),
              nullptr);
    for (const auto &slot : slots) {
        EXPECT_EQ(scene->getState().getComponentByUuid(slot->getUuid()),
                  nullptr);
        EXPECT_NE(moduleScene->getState().getComponentByUuid(slot->getUuid()),
                  nullptr);
    }

    Bess::SceneUIPrepareCtx moduleCtx{
        .sceneState = &moduleScene->getState(),
        .renderer = renderer,
        .parentNode = nullptr,
        .theme = Bess::Core::Style::BessTheme::defaultTheme(),
    };
    clock->prepareUI(moduleCtx);
    EXPECT_EQ(countUiComps(moduleScene->getState()), 12u);
    EXPECT_EQ(countUiComps(scene->getState()), 0u);

    ASSERT_TRUE(session->undo());
    EXPECT_EQ(countUiComps(moduleScene->getState()), 0u);
    EXPECT_NE(scene->getState().getComponentByUuid(clock->getUuid()), nullptr);

    session->clearHist();
    scene->clear();
    {
        py::gil_scoped_acquire gil;
        clock.reset();
        clockDefinition.reset();
    }
}

TEST_F(MainPageConnectionCommandsTest,
       AddComponentRedoRecreatesPreparedUiHelpers) {
    const auto fixture = addSimComponent(makeDefinition("JK Flip Flop", 4, 2));
    ASSERT_NE(fixture.comp, nullptr);

    const auto renderer = std::make_shared<TestRenderer2D>();
    Bess::SceneUIPrepareCtx prepareCtx{
        .sceneState = &scene->getState(),
        .renderer = renderer,
        .parentNode = nullptr,
        .theme = Bess::Core::Style::BessTheme::defaultTheme(),
    };

    fixture.comp->prepareUI(prepareCtx);

    ASSERT_TRUE(session->undo());
    ASSERT_EQ(scene->getState().getComponentByUuid(fixture.comp->getUuid()),
              nullptr);

    ASSERT_TRUE(session->redo());
    ASSERT_NE(scene->getState().getComponentByUuid(fixture.comp->getUuid()),
              nullptr);

    fixture.comp->prepareUI(prepareCtx);
    EXPECT_FALSE(fixture.comp->getUIDirty());
}

TEST_F(MainPageConnectionCommandsTest,
       DeleteModuleUndoRedoKeepsSceneAndSimulationInSync) {
    UUID inputId = UUID::null;
    UUID outputId = UUID::null;
    auto made = Bess::Canvas::ModuleSceneComponent::createNew(
        *sceneDriver, *simEngine, inputId, outputId);
    ASSERT_FALSE(made.empty());

    auto mod = std::dynamic_pointer_cast<Bess::Canvas::ModuleSceneComponent>(
        made.front());
    ASSERT_NE(mod, nullptr);
    made.erase(made.begin());

    auto modScene = sceneDriver->getSceneWithId(mod->getSceneId());
    ASSERT_NE(modScene, nullptr);
    ASSERT_TRUE(session->addComp(mod, std::move(made), scene->getSceneId()));

    const auto oldModSim = mod->getSimEngineId();
    const auto oldInput =
        modScene->getState()
            .getComponentByUuid<Bess::Canvas::SimulationSceneComponent>(
                mod->getAssociatedInp());
    const auto oldOutput =
        modScene->getState()
            .getComponentByUuid<Bess::Canvas::SimulationSceneComponent>(
                mod->getAssociatedOut());
    ASSERT_NE(oldInput, nullptr);
    ASSERT_NE(oldOutput, nullptr);
    const auto oldInputSim = oldInput->getSimEngineId();
    const auto oldOutputSim = oldOutput->getSimEngineId();
    ASSERT_NE(simEngine->getComponentDefinition(oldModSim), nullptr);
    ASSERT_NE(simEngine->getComponentDefinition(oldInputSim), nullptr);
    ASSERT_NE(simEngine->getComponentDefinition(oldOutputSim), nullptr);

    session->clearHist();
    auto tx = session->tx("Delete module");
    ASSERT_TRUE(Bess::Edit::rmModule(tx, scene, mod->getUuid()));
    ASSERT_TRUE(tx.commit());

    EXPECT_EQ(scene->getState().getComponentByUuid(mod->getUuid()), nullptr);
    EXPECT_EQ(sceneDriver->getSceneWithId(modScene->getSceneId()), nullptr);
    EXPECT_EQ(simEngine->getComponentDefinition(oldModSim), nullptr);
    EXPECT_EQ(simEngine->getComponentDefinition(oldInputSim), nullptr);
    EXPECT_EQ(simEngine->getComponentDefinition(oldOutputSim), nullptr);

    ASSERT_TRUE(session->undo());
    EXPECT_NE(scene->getState().getComponentByUuid(mod->getUuid()), nullptr);
    EXPECT_EQ(sceneDriver->getSceneWithId(modScene->getSceneId()), modScene);

    const auto input =
        modScene->getState()
            .getComponentByUuid<Bess::Canvas::SimulationSceneComponent>(
                mod->getAssociatedInp());
    const auto output =
        modScene->getState()
            .getComponentByUuid<Bess::Canvas::SimulationSceneComponent>(
                mod->getAssociatedOut());
    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);
    EXPECT_NE(simEngine->getComponentDefinition(mod->getSimEngineId()),
              nullptr);
    EXPECT_NE(simEngine->getComponentDefinition(input->getSimEngineId()),
              nullptr);
    EXPECT_NE(simEngine->getComponentDefinition(output->getSimEngineId()),
              nullptr);

    const auto def =
        std::dynamic_pointer_cast<Bess::SimEngine::ModuleDefinition>(
            mod->getCompDef());
    ASSERT_NE(def, nullptr);
    EXPECT_EQ(def->getInputId(), input->getSimEngineId());
    EXPECT_EQ(def->getOutputId(), output->getSimEngineId());

    const auto restoredModSim = mod->getSimEngineId();
    const auto restoredInputSim = input->getSimEngineId();
    const auto restoredOutputSim = output->getSimEngineId();
    ASSERT_TRUE(session->redo());
    EXPECT_EQ(scene->getState().getComponentByUuid(mod->getUuid()), nullptr);
    EXPECT_EQ(sceneDriver->getSceneWithId(modScene->getSceneId()), nullptr);
    EXPECT_EQ(simEngine->getComponentDefinition(restoredModSim), nullptr);
    EXPECT_EQ(simEngine->getComponentDefinition(restoredInputSim), nullptr);
    EXPECT_EQ(simEngine->getComponentDefinition(restoredOutputSim), nullptr);
}

TEST_F(MainPageConnectionCommandsTest, CopyPasteModulesFromSampleProject) {
    auto &pluginManager = Bess::Plugins::PluginManager::getInstance();
    ASSERT_TRUE(pluginManager.loadPluginsFromDirectory("plugins"));
    for (const auto &[name, plugin] : pluginManager.getLoadedPlugins()) {
        (void)name;
        for (const auto &definition : plugin->onCompCatalogLoad()) {
            Bess::SimEngine::ComponentCatalog::instance().registerComponent(
                definition);
        }
    }

    std::ifstream project("sample_projects/digital-clock.bproj");
    ASSERT_TRUE(project.is_open());
    Json::Value projectJson;
    Json::CharReaderBuilder reader;
    std::string parseErrors;
    ASSERT_TRUE(
        Json::parseFromStream(reader, project, &projectJson, &parseErrors))
        << parseErrors;

    simEngine->loadJson(projectJson["sim_engine_data"]);
    sceneDriver->removeScenes();

    const auto rootSceneId =
        projectJson["scene_data"]["root_scene_id"].asUInt64();
    Json::UInt64 moduleId = 0;
    Json::UInt64 moduleSceneId = 0;
    for (const auto &serializedScene : projectJson["scene_data"]["scenes"]) {
        const auto &stateJson = serializedScene["scene_state"];
        if (stateJson["sceneId"].asUInt64() != rootSceneId) {
            continue;
        }
        for (const auto &component : stateJson["components"]) {
            if (component["typeName"].asString() == "ModuleSceneComponent") {
                moduleId = component["uuid"].asUInt64();
                moduleSceneId = component["sceneId"].asUInt64();
                break;
            }
        }
    }
    ASSERT_NE(moduleId, 0u);
    ASSERT_NE(moduleSceneId, 0u);

    Bess::SceneSerializer serializer;
    for (const auto &serializedScene : projectJson["scene_data"]["scenes"]) {
        auto data = serializedScene;
        auto &stateJson = data["scene_state"];
        const auto sceneId = stateJson["sceneId"].asUInt64();
        if (sceneId != rootSceneId && sceneId != moduleSceneId) {
            continue;
        }

        if (sceneId == rootSceneId) {
            Json::Value filtered(Json::arrayValue);
            for (const auto &component : stateJson["components"]) {
                if (component["uuid"].asUInt64() == moduleId ||
                    component["parentComponent"].asUInt64() == moduleId) {
                    filtered.append(component);
                }
            }
            stateJson["components"] = std::move(filtered);
        }

        auto loadedScene = std::make_shared<Scene>(false);
        serializer.deserialize(
            data,
            loadedScene,
            Bess::Canvas::SceneLoadCtx{.sim = simEngine.get()});
        sceneDriver->addScene(loadedScene);
    }
    sceneDriver->setRootSceneId(rootSceneId);
    sceneDriver->setActiveScene(Bess::UUID(rootSceneId));
    scene = sceneDriver->getActiveScene();
    ASSERT_NE(scene, nullptr);

    std::vector<std::shared_ptr<Bess::Canvas::ModuleSceneComponent>> modules;
    for (const auto &[id, component] : scene->getState().getAllComponents()) {
        (void)id;
        if (const auto module =
                std::dynamic_pointer_cast<Bess::Canvas::ModuleSceneComponent>(
                    component)) {
            modules.push_back(module);
        }
    }
    ASSERT_FALSE(modules.empty());

    for (const auto &module : modules) {
        scene->getState().clearSelectedComponents();
        scene->getState().addSelectedComponent(module->getUuid());
        Bess::Svc::CopyPaste::Context copyPaste;
        copyPaste.copy(scene);
        const auto clonedIds = copyPaste.paste(scene, {200.f, 100.f}, false);
        EXPECT_TRUE(clonedIds.contains(module->getUuid()));
    }
}

TEST_F(MainPageConnectionCommandsTest,
       CopyPasteInvalidModuleFailsWithoutCreatingScene) {
    const auto fixture = addSimComponent(sourceDef);
    ASSERT_NE(fixture.comp, nullptr);
    const UUID net;
    fixture.comp->setNetId(net);

    const auto made =
        Bess::Edit::makeModule(*session, scene, net, "Invalid module");
    ASSERT_TRUE(made) << made.status.msg();
    ASSERT_NE(made.val, nullptr);

    const auto associatedInput = made.val->getAssociatedInp();
    made.val->setAssociatedInp(UUID::null);
    const auto sceneCount = sceneDriver->getSceneCount();

    scene->getState().addSelectedComponent(made.val->getUuid());
    Bess::Svc::CopyPaste::Context copyPaste;
    copyPaste.copy(scene);
    const auto clonedIds = copyPaste.paste(scene, {200.f, 100.f}, false);

    EXPECT_FALSE(clonedIds.contains(made.val->getUuid()));
    EXPECT_EQ(sceneDriver->getSceneCount(), sceneCount);
    made.val->setAssociatedInp(associatedInput);
}

TEST_F(MainPageConnectionCommandsTest,
       ModuleBoundarySlotGrowthSyncsEnclosingModuleSlots) {
    UUID inputId = UUID::null;
    UUID outputId = UUID::null;
    auto made = Bess::Canvas::ModuleSceneComponent::createNew(
        *sceneDriver, *simEngine, inputId, outputId);
    ASSERT_FALSE(made.empty());

    auto module = std::dynamic_pointer_cast<Bess::Canvas::ModuleSceneComponent>(
        made.front());
    ASSERT_NE(module, nullptr);
    made.erase(made.begin());
    ASSERT_TRUE(session->addComp(module, std::move(made), scene->getSceneId()));

    const auto moduleScene = sceneDriver->getSceneWithId(module->getSceneId());
    ASSERT_NE(moduleScene, nullptr);
    const auto moduleInput =
        moduleScene->getState()
            .getComponentByUuid<Bess::Canvas::SimulationSceneComponent>(
                module->getAssociatedInp());
    const auto moduleOutput =
        moduleScene->getState()
            .getComponentByUuid<Bess::Canvas::SimulationSceneComponent>(
                module->getAssociatedOut());
    ASSERT_NE(moduleInput, nullptr);
    ASSERT_NE(moduleOutput, nullptr);

    const auto moduleSim =
        simEngine->getComponent<Bess::SimEngine::Drivers::Digital::DigSimComp>(
            module->getSimEngineId());
    ASSERT_NE(moduleSim, nullptr);
    const auto moduleDef = moduleSim->getDefinition<DigCompDef>();
    ASSERT_NE(moduleDef, nullptr);
    ASSERT_EQ(moduleDef->getInputSlotsInfo().count, 1u);
    ASSERT_EQ(moduleDef->getOutputSlotsInfo().count, 1u);
    ASSERT_EQ(module->getInputSlots().size(), 1u);
    ASSERT_EQ(module->getOutputSlots().size(), 1u);

    ASSERT_TRUE(
        simEngine->addPort({.componentId = moduleInput->getSimEngineId(),
                            .direction = Bess::SimEngine::PortDirection::output,
                            .signalKind = Bess::SimEngine::SignalKind::digital,
                            .index = 1}));
    EXPECT_EQ(moduleDef->getInputSlotsInfo().count, 2u);
    ASSERT_EQ(module->getInputSlots().size(), 2u);
    const auto newModuleInputSlot =
        scene->getState().getComponentByUuid<SlotSceneComponent>(
            module->getInputSlots().back());
    ASSERT_NE(newModuleInputSlot, nullptr);
    EXPECT_TRUE(newModuleInputSlot->isInputSlot());
    EXPECT_EQ(newModuleInputSlot->getIndex(), 1);

    ASSERT_TRUE(
        simEngine->addPort({.componentId = moduleOutput->getSimEngineId(),
                            .direction = Bess::SimEngine::PortDirection::input,
                            .signalKind = Bess::SimEngine::SignalKind::digital,
                            .index = 1}));
    EXPECT_EQ(moduleDef->getOutputSlotsInfo().count, 2u);
    ASSERT_EQ(module->getOutputSlots().size(), 2u);
    const auto newModuleOutputSlot =
        scene->getState().getComponentByUuid<SlotSceneComponent>(
            module->getOutputSlots().back());
    ASSERT_NE(newModuleOutputSlot, nullptr);
    EXPECT_FALSE(newModuleOutputSlot->isInputSlot());
    EXPECT_EQ(newModuleOutputSlot->getIndex(), 1);
}

TEST_F(MainPageConnectionCommandsTest,
       InputResizeSlotCreatesControlForNewSlotSignalKind) {
    auto definition = Bess::SimEngine::Drivers::Math::MathCompDef::makeFunction(
        "Scalar Input",
        "Math",
        [](Bess::TimeMs, const std::vector<double> &) { return 0.0; },
        false);
    definition->setBehaviorType(Bess::SimEngine::ComponentBehaviorType::input);
    definition->setInputPortDescriptor({
        .direction = Bess::SimEngine::PortDirection::input,
        .signalKind = Bess::SimEngine::SignalKind::scalar,
        .quantityKind = Bess::SimEngine::QuantityKind::dimensionless,
        .count = 0,
    });
    definition->setOutputPortDescriptor({
        .direction = Bess::SimEngine::PortDirection::output,
        .signalKind = Bess::SimEngine::SignalKind::scalar,
        .quantityKind = Bess::SimEngine::QuantityKind::dimensionless,
        .count = 1,
        .names = {"x"},
        .isResizeable = true,
    });

    const auto fixture = addSimComponent(definition);
    const auto inputComp =
        std::dynamic_pointer_cast<Bess::Canvas::InputSceneComponent>(
            fixture.comp);
    ASSERT_NE(inputComp, nullptr);

    auto resizeSlot = std::shared_ptr<SlotSceneComponent>{};
    for (const auto &slot : fixture.outputs) {
        if (slot && slot->isResizeSlot()) {
            resizeSlot = slot;
            break;
        }
    }
    ASSERT_NE(resizeSlot, nullptr);
    ASSERT_EQ(resizeSlot->getSignalKind(), Bess::SimEngine::SignalKind::scalar);

    auto firstScalarSlot = std::shared_ptr<SlotSceneComponent>{};
    for (const auto &slot : fixture.outputs) {
        if (slot && !slot->isResizeSlot()) {
            firstScalarSlot = slot;
            break;
        }
    }
    ASSERT_NE(firstScalarSlot, nullptr);
    EXPECT_EQ(firstScalarSlot->getSignalKind(),
              Bess::SimEngine::SignalKind::scalar);
    EXPECT_TRUE(firstScalarSlot->getSlotState(scene->getState()).isScalar());

    const auto renderer = std::make_shared<TestRenderer2D>();
    Bess::SceneUIPrepareCtx prepareCtx{
        .sceneState = &scene->getState(),
        .renderer = renderer,
        .parentNode = nullptr,
        .theme = Bess::Core::Style::BessTheme::defaultTheme(),
    };
    inputComp->prepareUI(prepareCtx);

    auto sinkDefinition =
        std::make_shared<Bess::SimEngine::Drivers::Math::MathCompDef>();
    sinkDefinition->setName("Scalar Sink");
    sinkDefinition->setGroupName("Math");
    sinkDefinition->setInputPortDescriptor({
        .direction = Bess::SimEngine::PortDirection::input,
        .signalKind = Bess::SimEngine::SignalKind::scalar,
        .quantityKind = Bess::SimEngine::QuantityKind::dimensionless,
        .count = 1,
        .names = {"x"},
    });
    sinkDefinition->setOutputPortDescriptor({
        .direction = Bess::SimEngine::PortDirection::output,
        .signalKind = Bess::SimEngine::SignalKind::scalar,
        .quantityKind = Bess::SimEngine::QuantityKind::dimensionless,
        .count = 0,
    });
    sinkDefinition->setSimFn(
        [](const std::shared_ptr<
            Bess::SimEngine::Drivers::Math::MathCompSimData> &data) {
            return data;
        });

    const auto sinkFixture = addSimComponent(sinkDefinition);
    ASSERT_FALSE(sinkFixture.inputs.empty());

    const auto connection = connectionService->createConnection(
        resizeSlot->getUuid(), sinkFixture.inputs.front()->getUuid(), scene);
    ASSERT_NE(connection, nullptr);

    auto newSlot = std::shared_ptr<SlotSceneComponent>{};
    for (const auto &slotId : inputComp->getOutputSlots()) {
        const auto slot =
            scene->getState().getComponentByUuidSP<SlotSceneComponent>(slotId);
        if (slot && !slot->isResizeSlot() &&
            containsUuid(slot->getConnectedConnections(),
                         connection->getUuid())) {
            newSlot = slot;
            break;
        }
    }
    ASSERT_NE(newSlot, nullptr);
    EXPECT_FALSE(newSlot->isResizeSlot());
    EXPECT_EQ(newSlot->getSignalKind(), Bess::SimEngine::SignalKind::scalar);
    EXPECT_TRUE(newSlot->getSlotState(scene->getState()).isScalar());

    inputComp->prepareUI(prepareCtx);

    size_t textBoxCount = 0;
    size_t toggleCount = 0;
    for (const auto &depId : inputComp->getDependants(scene->getState())) {
        if (scene->getState()
                .getComponentByUuidSP<Bess::Canvas::UI::TextBoxComp>(depId)) {
            ++textBoxCount;
        }
        if (scene->getState()
                .getComponentByUuidSP<Bess::Canvas::UI::ToggleBtnComp>(depId)) {
            ++toggleCount;
        }
    }

    EXPECT_EQ(textBoxCount, 2u);
    EXPECT_EQ(toggleCount, 0u);
}

TEST_F(MainPageConnectionCommandsTest,
       ScalarSlotShowsInlineTextBoxOnlyWhileUnconnected) {
    const auto sinkFixture =
        addSimComponent(makeScalarDefinition("Scalar Sink", 1, 0));
    ASSERT_FALSE(sinkFixture.inputs.empty());

    const auto renderer = std::make_shared<TestRenderer2D>();
    Bess::SceneUIPrepareCtx prepareCtx{
        .sceneState = &scene->getState(),
        .renderer = renderer,
        .parentNode = nullptr,
        .theme = Bess::Core::Style::BessTheme::defaultTheme(),
    };

    sinkFixture.comp->prepareUI(prepareCtx);
    EXPECT_EQ(countScalarInputs(scene->getState()), 1u);

    const auto sourceFixture =
        addSimComponent(makeScalarDefinition("Scalar Source", 0, 1));
    ASSERT_FALSE(sourceFixture.outputs.empty());

    sourceFixture.comp->prepareUI(prepareCtx);
    EXPECT_EQ(countScalarInputs(scene->getState()), 1u);

    const auto connection = connectionService->createConnection(
        sourceFixture.outputs.front()->getUuid(),
        sinkFixture.inputs.front()->getUuid(),
        scene);
    ASSERT_NE(connection, nullptr);

    sinkFixture.comp->prepareUI(prepareCtx);
    EXPECT_EQ(countScalarInputs(scene->getState()), 0u);
}

TEST_F(MainPageConnectionCommandsTest,
       ScalarSlotInlineTextBoxUpdatesUnconnectedPortState) {
    const auto sinkFixture =
        addSimComponent(makeScalarDefinition("Scalar Sink", 1, 0));
    ASSERT_FALSE(sinkFixture.inputs.empty());
    const auto slot = sinkFixture.inputs.front();

    const auto renderer = std::make_shared<TestRenderer2D>();
    Bess::SceneUIPrepareCtx prepareCtx{
        .sceneState = &scene->getState(),
        .renderer = renderer,
        .parentNode = nullptr,
        .theme = Bess::Core::Style::BessTheme::defaultTheme(),
    };

    sinkFixture.comp->prepareUI(prepareCtx);
    measureSceneUi(scene->getState());

    const auto scalarInput = firstScalarInput(scene->getState());
    ASSERT_NE(scalarInput, nullptr);

    Bess::SceneDrawContext drawCtx{
        .sceneState = &scene->getState(),
        .renderer = renderer,
    };

    scalarInput->draw(drawCtx);
    const auto pointerPos = glm::vec2(scalarInput->getUINode()->getDrawPos());
    scalarInput->onFocusGained({
        .entityUuid = scalarInput->getUuid(),
        .mousePos = pointerPos,
        .sceneState = &scene->getState(),
    });

    const auto evt = textInputEvent(U'4');
    EXPECT_TRUE(scalarInput->onKeyEvent(evt));
    scalarInput->draw(drawCtx);

    const auto slotState = slot->getSlotState(scene->getState());
    ASSERT_TRUE(slotState.isScalar());
    EXPECT_DOUBLE_EQ(slotState.scalarValue, 4.0);
}

TEST_F(MainPageConnectionCommandsTest,
       MonitorPlotsStampedSamplesFromMathComponents) {
    const auto definition =
        Bess::SimEngine::Drivers::Math::MathCompDef::makeFunction(
            "Monitored Sine",
            "Test",
            [](Bess::TimeMs time, const std::vector<double> &) {
                return std::sin(time.count());
            },
            true,
            Bess::TimeNs(1e6));
    const auto source = addSimComponent(definition);
    ASSERT_NE(source.comp, nullptr);
    ASSERT_FALSE(source.outputs.empty());

    auto monitor = std::make_shared<Bess::Canvas::MonitorSceneComp>();
    ASSERT_TRUE(session->addComp(monitor, {}, scene->getSceneId()));
    const auto slotId = source.outputs.front()->getUuid();
    monitor->addSlotProbe(scene->getState(), slotId);

    simEngine->runFor(Bess::TimeMs(4), Bess::TimeMs(2));
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
    while (simEngine->getSimulationState() !=
               Bess::SimEngine::SimulationState::stopped &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_EQ(simEngine->getSimulationState(),
              Bess::SimEngine::SimulationState::stopped);
    simEngine->stop();

    {
        const auto stampData = simEngine->getStampData();
        const auto stampHistory =
            stampData.find(source.comp->getSimEngineId());
        ASSERT_TRUE(stampHistory.has_value());
        ASSERT_EQ(stampHistory->samples.size(), 3U);
    }

    monitor->update(Bess::TimeMs(0), scene->getState());
    const auto renderer = std::make_shared<TestRenderer2D>();
    Bess::SceneDrawContext drawContext{
        .sceneState = &scene->getState(),
        .renderer = renderer,
    };
    monitor->draw(drawContext);

    const auto tracePathCount = std::ranges::count_if(
        renderer->paths,
        [runtimeId = monitor->getRuntimeId()](const auto &path) {
            return path.id.runtimeId == runtimeId && path.id.info >= 3U &&
                   path.id.info < 68U;
        });
    EXPECT_EQ(tracePathCount, 1);

    monitor->removeSlotProbe(scene->getState(), slotId);
}

TEST_F(MainPageConnectionCommandsTest,
       MonitorDecimatesAndBatchesLargeStampHistories) {
    const auto definition =
        Bess::SimEngine::Drivers::Math::MathCompDef::makeFunction(
            "Dense Monitor Source",
            "Test",
            [](Bess::TimeMs, const std::vector<double> &) { return 0.0; },
            false);
    const auto source = addSimComponent(definition);
    ASSERT_NE(source.comp, nullptr);
    ASSERT_FALSE(source.outputs.empty());

    auto monitor = std::make_shared<Bess::Canvas::MonitorSceneComp>();
    ASSERT_TRUE(session->addComp(monitor, {}, scene->getSceneId()));
    monitor->addSlotProbe(scene->getState(), source.outputs.front()->getUuid());

    const auto driver = simEngine->getDriverWithName(
        Bess::SimEngine::Drivers::Math::MathSimDriver::NAME);
    ASSERT_NE(driver, nullptr);
    driver->clearStampData();

    constexpr std::size_t sampleCount = 5000;
    for (std::size_t i = 0; i < sampleCount; ++i) {
        const auto simTime = Bess::TimeNs(static_cast<double>(i));
        simEngine->setOutputPortState(
            source.comp->getSimEngineId(),
            0,
            Bess::SimEngine::PortState::scalar(
                static_cast<double>(i % 17U), simTime));
        driver->stampSim(simTime, true);
    }

    monitor->update(Bess::TimeMs(0), scene->getState());
    const auto renderer = std::make_shared<TestRenderer2D>();
    Bess::SceneDrawContext drawContext{
        .sceneState = &scene->getState(),
        .renderer = renderer,
    };
    monitor->draw(drawContext);

    const auto tracePathCount = std::ranges::count_if(
        renderer->paths,
        [runtimeId = monitor->getRuntimeId()](const auto &path) {
            return path.id.runtimeId == runtimeId && path.id.info >= 3U &&
                   path.id.info < 68U;
        });
    EXPECT_EQ(tracePathCount, 1);
    EXPECT_GT(renderer->pathLineCount, 0U);
    EXPECT_LT(renderer->pathLineCount, sampleCount);
}

TEST_F(MainPageConnectionCommandsTest,
       PythonSceneDrawDoesNotDeadlockWithSimulationCallback) {
    simEngine->clear(false);

    std::atomic<bool> callbackEntered{false};
    std::atomic<bool> callbackCompleted{false};
    auto definition = makeDefinition("Python GIL Lock Ordering", 0, 1);
    definition->setAutoReschedule(true);
    definition->setAutoRescheduleDelay(Bess::TimeNs(1e9));
    definition->setSimFn(
        [&](const std::shared_ptr<
            Bess::SimEngine::Drivers::Digital::DigCompSimData> &data) {
            callbackEntered.store(true, std::memory_order_release);

            // Python-defined clocks enter Python here while the scheduler
            // owns the engine execution barrier.
            py::gil_scoped_acquire gil;
            callbackCompleted.store(true, std::memory_order_release);
            data->outputStates[0] = Bess::SimEngine::PortState::digital(
                Bess::SimEngine::LogicState::high, data->simTime);
            return data;
        });

    const auto fixture = addSimComponent(definition);
    ASSERT_NE(fixture.comp, nullptr);
    ASSERT_NE(fixture.comp->getSimEngineId(), UUID::null);

    const auto renderer = std::make_shared<TestRenderer2D>();
    Bess::SceneUIPrepareCtx prepareContext{
        .sceneState = &scene->getState(),
        .renderer = renderer,
        .theme = Bess::Core::Style::BessTheme::defaultTheme(),
    };
    fixture.comp->prepareUI(prepareContext);
    measureSceneUi(scene->getState());

    Bess::SimDrawCache simDrawCache;
    simDrawCache.setSimEngine(simEngine);
    Bess::SceneDrawContext drawContext{
        .sceneState = &scene->getState(),
        .renderer = renderer,
        .simDrawCache = &simDrawCache,
    };

    {
        py::gil_scoped_acquire gil;
        (void)py::module_::import("bessplug.api.scene");
        auto pythonComponent = py::cast(fixture.comp);
        auto pythonDrawContext = py::cast(
            &drawContext, py::return_value_policy::reference);
        simEngine->run();

        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
        while (!callbackEntered.load(std::memory_order_acquire) &&
               std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        ASSERT_TRUE(callbackEntered.load(std::memory_order_acquire));

        // This is the application's exact draw path: the Python Clock scene
        // component owns the GIL, then its C++ draw_slots implementation
        // queries simulation state while the scheduler callback waits for
        // that GIL.
        pythonComponent.attr("draw_slots")(pythonDrawContext);
    }

    EXPECT_TRUE(callbackCompleted.load(std::memory_order_acquire));
    simEngine->stop();
}

TEST_F(MainPageConnectionCommandsTest,
       PythonClockAndMathSineRemainResponsiveAcrossRunModes) {
    simEngine->clear(false);
    auto &pluginManager = Bess::Plugins::PluginManager::getInstance();
    ASSERT_TRUE(pluginManager.loadPluginsFromDirectory("plugins"));

    std::shared_ptr<Bess::SimEngine::Drivers::CompDef> clockDefinition;
    {
        py::gil_scoped_acquire gil;
        auto clockModule = py::module_::import("components.clock");
        auto pythonClockDefinition = clockModule.attr("ClockDefinition")();
        pythonClockDefinition.attr("frequency") = 2000.0;
        pythonClockDefinition.attr("duty_cycle") = 0.25;
        clockDefinition =
            pythonClockDefinition
                .cast<std::shared_ptr<Bess::SimEngine::Drivers::CompDef>>();

        const auto eventDefinition = std::dynamic_pointer_cast<
            Bess::SimEngine::Drivers::EvtBasedCompDef>(clockDefinition);
        ASSERT_NE(eventDefinition, nullptr);
        EXPECT_EQ(eventDefinition->getInitialSimDelay(), Bess::TimeNs(375000));
        EXPECT_EQ(eventDefinition->getSelfSimDelayAfter(1),
                  Bess::TimeNs(125000));
        EXPECT_EQ(eventDefinition->getSelfSimDelayAfter(2),
                  Bess::TimeNs(375000));
    }
    ASSERT_NE(clockDefinition, nullptr);

    const auto clock = addSimComponent(clockDefinition);
    ASSERT_NE(clock.comp, nullptr);

    const auto sineDefinition =
        Bess::SimEngine::Drivers::Math::MathCompDef::makeFunction(
            "Normal Run Sine",
            "Timing Test",
            [](Bess::TimeMs time, const std::vector<double> &) {
                return std::sin(time.count() * 0.01);
            },
            true,
            Bess::TimeNs(5e5));
    const auto sine = addSimComponent(sineDefinition);
    ASSERT_NE(sine.comp, nullptr);

    simEngine->runFor(Bess::TimeMs(1), Bess::TimeMs(0.125));
    const auto phaseDeadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (simEngine->getSimulationState() !=
               Bess::SimEngine::SimulationState::stopped &&
           std::chrono::steady_clock::now() < phaseDeadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_EQ(simEngine->getSimulationState(),
              Bess::SimEngine::SimulationState::stopped);
    {
        const auto stampData = simEngine->getStampData();
        const auto history = stampData.find(clock.comp->getSimEngineId());
        ASSERT_TRUE(history.has_value());
        ASSERT_FALSE(history->samples.empty());
        ASSERT_EQ(history->samples.front().simTime, Bess::TimeNs(0));
        ASSERT_EQ(history->samples.front().outputStates.size(), 1);
        EXPECT_EQ(history->samples.front().outputStates[0].getLogicState(),
                  Bess::SimEngine::LogicState::low);

        const auto firstHigh = std::ranges::find_if(
            history->samples, [](const auto &sample) {
                return !sample.outputStates.empty() &&
                       sample.outputStates[0].getLogicState() ==
                           Bess::SimEngine::LogicState::high;
            });
        ASSERT_NE(firstHigh, history->samples.end());
        EXPECT_EQ(firstHigh->simTime, Bess::TimeNs(375000));
    }

    const auto renderer = std::make_shared<TestRenderer2D>();
    Bess::SceneUIPrepareCtx prepareContext{
        .sceneState = &scene->getState(),
        .renderer = renderer,
        .theme = Bess::Core::Style::BessTheme::defaultTheme(),
    };
    clock.comp->prepareUI(prepareContext);
    sine.comp->prepareUI(prepareContext);
    measureSceneUi(scene->getState());

    Bess::SimDrawCache simDrawCache;
    simDrawCache.setSimEngine(simEngine);
    Bess::SceneDrawContext drawContext{
        .sceneState = &scene->getState(),
        .renderer = renderer,
        .simDrawCache = &simDrawCache,
    };

    py::object pythonClockComponent;
    py::object pythonDrawContext;
    {
        py::gil_scoped_acquire gil;
        (void)py::module_::import("bessplug.api.scene");
        pythonClockComponent = py::cast(clock.comp);
        pythonDrawContext =
            py::cast(&drawContext, py::return_value_policy::reference);
    }

    for (int restart = 0; restart < 5; ++restart) {
        simEngine->run();
        const auto runDeadline = std::chrono::steady_clock::now() +
                                 std::chrono::milliseconds(300);
        while (std::chrono::steady_clock::now() < runDeadline) {
            simDrawCache.clear();
            {
                py::gil_scoped_acquire gil;
                pythonClockComponent.attr("draw_slots")(pythonDrawContext);
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }

        EXPECT_EQ(simEngine->getSimulationState(),
                  Bess::SimEngine::SimulationState::running);
        EXPECT_GE(simEngine->getCurrentSimTime(), Bess::TimeNs(5e7));

        const auto stopStarted = std::chrono::steady_clock::now();
        simEngine->stop();
        EXPECT_LT(std::chrono::steady_clock::now() - stopStarted,
                  std::chrono::milliseconds(250));
    }

    simEngine->runFor(Bess::TimeMs(1000), Bess::TimeMs(1));
    const auto timedDeadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (simEngine->getSimulationState() !=
               Bess::SimEngine::SimulationState::stopped &&
           std::chrono::steady_clock::now() < timedDeadline) {
        simDrawCache.clear();
        {
            py::gil_scoped_acquire gil;
            pythonClockComponent.attr("draw_slots")(pythonDrawContext);
        }
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    ASSERT_EQ(simEngine->getSimulationState(),
              Bess::SimEngine::SimulationState::stopped);
    simEngine->stop();
    EXPECT_DOUBLE_EQ(simEngine->getCurrentSimTime().count(), 1e9);

    {
        py::gil_scoped_acquire gil;
        pythonDrawContext = py::object();
        pythonClockComponent = py::object();
    }
}

TEST_F(MainPageConnectionCommandsTest,
       DeleteComponentUndoRestoresItsConnections) {
    const auto source = addSimComponent(sourceDef);
    const auto sink = addSimComponent(sinkDef);
    ASSERT_NE(source.comp, nullptr);
    ASSERT_NE(sink.comp, nullptr);
    ASSERT_FALSE(source.outputs.empty());
    ASSERT_FALSE(sink.inputs.empty());

    auto connection =
        connectionService->createConnection(source.outputs.front()->getUuid(),
                                            sink.inputs.front()->getUuid(),
                                            scene);
    ASSERT_NE(connection, nullptr);
    expectConnectionRestored(source, sink, connection);

    ASSERT_TRUE(session->rmComp(source.comp->getUuid(), scene->getSceneId()));

    EXPECT_TRUE(session->canUndo());
    EXPECT_EQ(scene->getState().getComponentByUuid(source.comp->getUuid()),
              nullptr);
    EXPECT_EQ(
        scene->getState().getComponentByUuid(source.outputs.front()->getUuid()),
        nullptr);
    EXPECT_EQ(scene->getState().getComponentByUuid(connection->getUuid()),
              nullptr);
    EXPECT_FALSE(containsUuid(sink.inputs.front()->getConnectedConnections(),
                              connection->getUuid()));

    ASSERT_TRUE(session->undo());
    expectConnectionRestored(source, sink, connection);

    ASSERT_TRUE(session->canRedo());
    ASSERT_TRUE(session->redo());
    EXPECT_EQ(scene->getState().getComponentByUuid(source.comp->getUuid()),
              nullptr);
    EXPECT_EQ(scene->getState().getComponentByUuid(connection->getUuid()),
              nullptr);
    EXPECT_FALSE(containsUuid(sink.inputs.front()->getConnectedConnections(),
                              connection->getUuid()));

    ASSERT_TRUE(session->canUndo());
    ASSERT_TRUE(session->undo());
    expectConnectionRestored(source, sink, connection);
}

TEST_F(MainPageConnectionCommandsTest,
       DeleteConnectionUndoRestoresRemovedResizableSlotInSimEngine) {
    const auto resizableSourceDef =
        makeDefinition("Resizable Source", 0, 2, false, true);
    const auto source = addSimComponent(resizableSourceDef);
    const auto sink = addSimComponent(sinkDef);

    ASSERT_NE(source.comp, nullptr);
    ASSERT_NE(sink.comp, nullptr);
    ASSERT_GE(source.outputs.size(), 3u);
    ASSERT_FALSE(sink.inputs.empty());

    auto sourceSlot = source.outputs[1];
    ASSERT_FALSE(sourceSlot->isResizeSlot());
    ASSERT_EQ(sourceSlot->getIndex(), 1);
    ASSERT_EQ(simSlotCount(source, false), 2u);

    auto connection = connectionService->createConnection(
        sourceSlot->getUuid(), sink.inputs.front()->getUuid(), scene);
    ASSERT_NE(connection, nullptr);
    EXPECT_TRUE(containsUuid(sourceSlot->getConnectedConnections(),
                             connection->getUuid()));

    ASSERT_TRUE(session->rmComp(connection->getUuid(), scene->getSceneId()));

    EXPECT_TRUE(session->canUndo());
    EXPECT_EQ(scene->getState().getComponentByUuid(connection->getUuid()),
              nullptr);
    EXPECT_EQ(scene->getState().getComponentByUuid(sourceSlot->getUuid()),
              nullptr);
    EXPECT_EQ(simSlotCount(source, false), 1u);
    EXPECT_FALSE(containsUuid(sink.inputs.front()->getConnectedConnections(),
                              connection->getUuid()));

    ASSERT_TRUE(session->undo());

    EXPECT_NE(scene->getState().getComponentByUuid(sourceSlot->getUuid()),
              nullptr);
    EXPECT_NE(scene->getState().getComponentByUuid(connection->getUuid()),
              nullptr);
    EXPECT_EQ(simSlotCount(source, false), 2u);
    EXPECT_EQ(sourceSlot->getIndex(), 1);
    EXPECT_TRUE(containsUuid(sourceSlot->getConnectedConnections(),
                             connection->getUuid()));
    EXPECT_TRUE(containsUuid(sink.inputs.front()->getConnectedConnections(),
                             connection->getUuid()));
}

TEST_F(MainPageConnectionCommandsTest,
       DeleteConnectionUndoDoesNotDuplicateNotGatePairedOutputSlot) {
    Bess::Pages::MainPageState mainPageState;
    mainPageState.init();

    const auto notDef =
        makeDefinition("NOT Gate", 1, 1, true, false, true, '!');
    const auto source = addSimComponent(sourceDef);
    const auto notGate = addSimComponent(notDef);

    ASSERT_NE(source.comp, nullptr);
    ASSERT_NE(notGate.comp, nullptr);
    ASSERT_FALSE(source.outputs.empty());
    ASSERT_GE(notGate.inputs.size(), 2u);

    auto dispatcher = Bess::GAppContext::getInstance()
                          .getSubSystem<Bess::EventSystem::EventDispatcher>();
    dispatcher->dispatchAll();

    const auto resizeInput = notGate.inputs.back();
    ASSERT_TRUE(resizeInput->isResizeSlot());

    auto connection = connectionService->createConnection(
        source.outputs.front()->getUuid(), resizeInput->getUuid(), scene);
    ASSERT_NE(connection, nullptr);
    dispatcher->dispatchAll();

    const auto restoredInputId = connection->getEndSlot();
    ASSERT_NE(restoredInputId, resizeInput->getUuid());
    const auto restoredInput =
        scene->getState().getComponentByUuid<SlotSceneComponent>(
            restoredInputId);
    ASSERT_NE(restoredInput, nullptr);
    ASSERT_EQ(restoredInput->getIndex(), 1);
    ASSERT_EQ(simSlotCount(notGate, true), 2u);
    ASSERT_EQ(simSlotCount(notGate, false), 2u);
    ASSERT_EQ(sceneRealSlotCount(notGate, true), 2u);
    ASSERT_EQ(sceneRealSlotCount(notGate, false), 2u);
    ASSERT_GE(notGate.comp->getOutputSlots().size(), 2u);

    const auto pairedOutputId = notGate.comp->getOutputSlots()[1];
    const auto outputCountBeforeDelete = notGate.comp->getOutputSlots().size();
    ASSERT_NE(scene->getState().getComponentByUuid<SlotSceneComponent>(
                  pairedOutputId),
              nullptr);

    ASSERT_TRUE(session->rmComp(connection->getUuid(), scene->getSceneId()));
    dispatcher->dispatchAll();

    EXPECT_EQ(scene->getState().getComponentByUuid(connection->getUuid()),
              nullptr);
    EXPECT_EQ(scene->getState().getComponentByUuid(restoredInputId), nullptr);
    EXPECT_EQ(scene->getState().getComponentByUuid(pairedOutputId), nullptr);
    EXPECT_EQ(simSlotCount(notGate, true), 1u);
    EXPECT_EQ(simSlotCount(notGate, false), 1u);
    EXPECT_EQ(sceneRealSlotCount(notGate, true), 1u);
    EXPECT_EQ(sceneRealSlotCount(notGate, false), 1u);

    ASSERT_TRUE(session->canUndo());
    ASSERT_TRUE(session->undo());
    dispatcher->dispatchAll();

    EXPECT_NE(scene->getState().getComponentByUuid(restoredInputId), nullptr);
    EXPECT_NE(scene->getState().getComponentByUuid(pairedOutputId), nullptr);
    EXPECT_NE(scene->getState().getComponentByUuid(connection->getUuid()),
              nullptr);
    EXPECT_EQ(simSlotCount(notGate, true), 2u);
    EXPECT_EQ(simSlotCount(notGate, false), 2u);
    EXPECT_EQ(sceneRealSlotCount(notGate, true), 2u);
    EXPECT_EQ(sceneRealSlotCount(notGate, false), 2u);
    EXPECT_EQ(notGate.comp->getOutputSlots().size(), outputCountBeforeDelete);
    EXPECT_TRUE(containsUuid(notGate.comp->getOutputSlots(), pairedOutputId));

    const auto outputIds = notGate.comp->getOutputSlots();
    const std::unordered_set<UUID> uniqueOutputIds(outputIds.begin(),
                                                   outputIds.end());
    EXPECT_EQ(uniqueOutputIds.size(), outputIds.size());
}
