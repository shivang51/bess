#include "bess_core/commands/add_component_command.h"
#include "bess_core/commands/command_system.h"
#include "bess_core/commands/delete_component_command.h"
#include "bess_core/commands/macro_command.h"
#include "bess_core/commands/update_value_command.h"
#include "bess_core/g_app_context.h"
#include "bess_core/scene/scene.h"
#include "bess_core/scene/scene_state/components/scene_component.h"
#include "event_dispatcher.h"
#include <gtest/gtest.h>
#include <memory>

namespace {
    class FailingCommand : public Bess::Cmd::Command {
      public:
        FailingCommand() {
            m_name = "FailingCommand";
        }

        bool execute(const Bess::Cmd::CommandContext &context) override {
            (void)context;
            return false;
        }

        void undo(const Bess::Cmd::CommandContext &context) override {
            (void)context;
        }

        void redo(const Bess::Cmd::CommandContext &context) override {
            (void)context;
        }
    };
} // namespace

class CoreSceneCommandsTest : public testing::Test {
  protected:
    void SetUp() override {
        auto &appCtx = Bess::GAppContext::getInstance();
        if (!appCtx.hasSubSystem<Bess::EventSystem::EventDispatcher>()) {
            appCtx.addSubSystem<Bess::EventSystem::EventDispatcher>();
        }

        scene = std::make_shared<Bess::Canvas::Scene>(false);
        commandSystem.setScene(scene);
        commandSystem.reset();
    }

    void TearDown() override {
        commandSystem.reset();
        if (scene) {
            scene->clear();
            scene.reset();
        }

        auto &appCtx = Bess::GAppContext::getInstance();
        if (appCtx.hasSubSystem<Bess::EventSystem::EventDispatcher>()) {
            appCtx.getSubSystem<Bess::EventSystem::EventDispatcher>()->clear();
        }
    }

    std::shared_ptr<Bess::Canvas::Scene> scene;
    Bess::Cmd::CommandSystem commandSystem;
};

TEST_F(CoreSceneCommandsTest, AddComponentCommandRestoresComponentTree) {
    auto root = std::make_shared<Bess::Canvas::SceneComponent>();
    root->setName("root");
    auto child = std::make_shared<Bess::Canvas::SceneComponent>();
    child->setName("child");

    commandSystem.execute(
        std::make_unique<Bess::Cmd::AddCompCmd<Bess::Canvas::SceneComponent>>(
            root,
            std::vector<std::shared_ptr<Bess::Canvas::SceneComponent>>{child}));

    ASSERT_TRUE(commandSystem.canUndo());
    EXPECT_NE(scene->getState().getComponentByUuid(root->getUuid()), nullptr);
    EXPECT_NE(scene->getState().getComponentByUuid(child->getUuid()), nullptr);
    EXPECT_EQ(child->getParentComponent(), root->getUuid());
    EXPECT_TRUE(root->getChildComponents().contains(child->getUuid()));

    commandSystem.undo();
    EXPECT_TRUE(scene->getState().getAllComponents().empty());
    ASSERT_TRUE(commandSystem.canRedo());

    commandSystem.redo();
    EXPECT_NE(scene->getState().getComponentByUuid(root->getUuid()), nullptr);
    EXPECT_NE(scene->getState().getComponentByUuid(child->getUuid()), nullptr);
    EXPECT_EQ(child->getParentComponent(), root->getUuid());
    EXPECT_TRUE(root->getChildComponents().contains(child->getUuid()));
}

TEST_F(CoreSceneCommandsTest, DeleteComponentCommandRestoresDependants) {
    auto root = std::make_shared<Bess::Canvas::SceneComponent>();
    auto child = std::make_shared<Bess::Canvas::SceneComponent>();

    scene->getState().addComponent(root, false, false);
    scene->getState().addComponent(child, false, false);
    scene->getState().attachChild(root->getUuid(), child->getUuid(), false);

    commandSystem.execute(std::make_unique<Bess::Cmd::DeleteCompCmd>(
        std::vector<Bess::UUID>{root->getUuid()}));

    EXPECT_TRUE(commandSystem.canUndo());
    EXPECT_TRUE(scene->getState().getAllComponents().empty());

    commandSystem.undo();
    EXPECT_NE(scene->getState().getComponentByUuid(root->getUuid()), nullptr);
    EXPECT_NE(scene->getState().getComponentByUuid(child->getUuid()), nullptr);
    EXPECT_EQ(child->getParentComponent(), root->getUuid());
    EXPECT_TRUE(root->getChildComponents().contains(child->getUuid()));

    commandSystem.redo();
    EXPECT_TRUE(scene->getState().getAllComponents().empty());
}

TEST_F(CoreSceneCommandsTest, UpdateValueCommandsMergeIntoSingleUndoStep) {
    glm::vec3 value{0.f, 0.f, 0.f};

    value = {1.f, 0.f, 0.f};
    commandSystem.push(std::make_unique<Bess::Cmd::UpdateValCommand<glm::vec3>>(
        &value, value, glm::vec3{0.f, 0.f, 0.f}));

    value = {2.f, 0.f, 0.f};
    commandSystem.push(std::make_unique<Bess::Cmd::UpdateValCommand<glm::vec3>>(
        &value, value, glm::vec3{1.f, 0.f, 0.f}));

    commandSystem.undo();
    EXPECT_EQ(value, glm::vec3(0.f, 0.f, 0.f));

    commandSystem.redo();
    EXPECT_EQ(value, glm::vec3(2.f, 0.f, 0.f));
}

TEST_F(CoreSceneCommandsTest, MacroCommandRollsBackExecutedCommandsOnFailure) {
    auto component = std::make_shared<Bess::Canvas::SceneComponent>();

    auto macro = std::make_unique<Bess::Cmd::MacroCommand>();
    macro->addCommand(
        std::make_unique<Bess::Cmd::AddCompCmd<Bess::Canvas::SceneComponent>>(
            component));
    macro->addCommand(std::make_unique<FailingCommand>());

    commandSystem.execute(std::move(macro));

    EXPECT_FALSE(commandSystem.canUndo());
    EXPECT_TRUE(scene->getState().getAllComponents().empty());
}
