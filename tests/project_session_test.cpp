#include "bess_core/g_app_context.h"
#include "bess_core/scene/scene.h"
#include "bess_core/scene/scene_state/components/scene_component.h"
#include "bess_core/scene/scene_ui/controls/container_comp.h"
#include "bess_core/scene/scene_ui/controls/label_comp.h"
#include "bess_core/scene_driver.h"
#include "event_dispatcher.h"
#include "project_session/project_session.h"

#include <gtest/gtest.h>
#include <json/writer.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <type_traits>
#include <vector>

static_assert(!std::is_copy_constructible_v<Bess::ProjectSession>);
static_assert(!std::is_copy_assignable_v<Bess::ProjectSession>);
static_assert(!std::is_move_constructible_v<Bess::ProjectSession>);
static_assert(!std::is_move_assignable_v<Bess::ProjectSession>);
static_assert(!std::is_copy_constructible_v<Bess::ProjectTx>);
static_assert(!std::is_copy_assignable_v<Bess::ProjectTx>);

namespace {
    class OrderedDepsComp final : public Bess::Canvas::SceneComponent {
      public:
        std::vector<Bess::UUID>
        getDependants(const Bess::Canvas::SceneState &state) const override {
            std::vector<Bess::UUID> deps;
            for (const auto childId : getChildComponents()) {
                const auto child = state.getComponentByUuid(childId);
                if (!child) {
                    continue;
                }
                const auto childDeps = child->getDependants(state);
                deps.insert(deps.end(), childDeps.begin(), childDeps.end());
                deps.push_back(childId);
            }
            return deps;
        }
    };

    class TmpDir {
      public:
        TmpDir() {
            path = std::filesystem::temp_directory_path() /
                   ("bess_session_test_" +
                    std::to_string(static_cast<std::uint64_t>(Bess::UUID{})));
            std::filesystem::create_directories(path);
        }

        ~TmpDir() {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }

        std::filesystem::path path;
    };
} // namespace

class ProjectSessionTest : public testing::Test {
  protected:
    void SetUp() override {
        auto &app = Bess::GAppContext::getInstance();
        if (!app.hasSubSystem<Bess::EventSystem::EventDispatcher>()) {
            app.addSubSystem<Bess::EventSystem::EventDispatcher>()->onInit();
        }
        if (app.hasSubSystem<Bess::ProjectSession>()) {
            app.getSubSystem<Bess::ProjectSession>()->onDestroy();
            app.removeSubSystem<Bess::ProjectSession>();
        }

        session = app.addSubSystem<Bess::ProjectSession>();
        session->onInit();
        scene = std::make_shared<Bess::Canvas::Scene>(false);
        scene->getState().setIsRootScene(true);
        session->scenes().addScene(scene);
        session->scenes().setRootSceneId(scene->getSceneId());
        session->scenes().setActiveScene(scene->getSceneId());
        session->clearHist();
    }

    void TearDown() override {
        if (session) {
            session->onDestroy();
            session.reset();
        }
        scene.reset();

        auto &app = Bess::GAppContext::getInstance();
        if (app.hasSubSystem<Bess::ProjectSession>()) {
            app.removeSubSystem<Bess::ProjectSession>();
        }
        if (app.hasSubSystem<Bess::EventSystem::EventDispatcher>()) {
            app.getSubSystem<Bess::EventSystem::EventDispatcher>()->clear();
        }
    }

    std::shared_ptr<Bess::ProjectSession> session;
    std::shared_ptr<Bess::Canvas::Scene> scene;
};

TEST_F(ProjectSessionTest, AddUndoRedoRestoresComponentTree) {
    auto root = std::make_shared<Bess::Canvas::SceneComponent>();
    root->setName("root");
    auto child = std::make_shared<Bess::Canvas::SceneComponent>();
    child->setName("child");

    const auto add = session->addComp(root, {child}, scene->getSceneId());
    ASSERT_TRUE(add) << add.status.msg();
    EXPECT_TRUE(session->canUndo());
    EXPECT_NE(scene->getState().getComponentByUuid(root->getUuid()), nullptr);
    EXPECT_NE(scene->getState().getComponentByUuid(child->getUuid()), nullptr);
    EXPECT_EQ(child->getParentComponent(), root->getUuid());
    EXPECT_TRUE(root->getChildComponents().contains(child->getUuid()));

    const auto undo = session->undo();
    ASSERT_TRUE(undo) << undo.status.msg();
    EXPECT_TRUE(scene->getState().getAllComponents().empty());
    EXPECT_TRUE(session->canRedo());

    const auto redo = session->redo();
    ASSERT_TRUE(redo) << redo.status.msg();
    EXPECT_NE(scene->getState().getComponentByUuid(root->getUuid()), nullptr);
    EXPECT_NE(scene->getState().getComponentByUuid(child->getUuid()), nullptr);
    EXPECT_EQ(child->getParentComponent(), root->getUuid());
    EXPECT_TRUE(root->getChildComponents().contains(child->getUuid()));
}

TEST_F(ProjectSessionTest, RemoveUndoRedoRestoresDependantsAndOrder) {
    auto root = std::make_shared<OrderedDepsComp>();
    auto first = std::make_shared<Bess::Canvas::SceneComponent>();
    auto second = std::make_shared<Bess::Canvas::SceneComponent>();

    auto &state = scene->getState();
    state.addComponent(root, false, false);
    state.addComponent(first, false, false);
    state.addComponent(second, false, false);
    state.attachChild(root->getUuid(), first->getUuid(), false);
    state.attachChild(root->getUuid(), second->getUuid(), false);

    const auto rm = session->rmComp(root->getUuid(), scene->getSceneId());
    ASSERT_TRUE(rm) << rm.status.msg();
    EXPECT_TRUE(state.getAllComponents().empty());

    const auto undo = session->undo();
    ASSERT_TRUE(undo) << undo.status.msg();
    const auto restored = state.getComponentByUuid(root->getUuid());
    ASSERT_NE(restored, nullptr);
    const std::vector<Bess::UUID> children{
        restored->getChildComponents().begin(),
        restored->getChildComponents().end(),
    };
    ASSERT_EQ(children.size(), 2u);
    EXPECT_EQ(children[0], first->getUuid());
    EXPECT_EQ(children[1], second->getUuid());

    const auto redo = session->redo();
    ASSERT_TRUE(redo) << redo.status.msg();
    EXPECT_TRUE(state.getAllComponents().empty());
}

TEST_F(ProjectSessionTest, MoveEditsMergeIntoOneUndoEntry) {
    auto comp = std::make_shared<Bess::Canvas::SceneComponent>();
    ASSERT_TRUE(session->addComp(comp, {}, scene->getSceneId()));
    const auto start = comp->getTransform().position;
    session->clearHist();

    const auto first = session->moveComp(
        comp->getUuid(), {10.f, 2.f, 0.f}, scene->getSceneId());
    ASSERT_TRUE(first) << first.status.msg();
    const auto second = session->moveComp(
        comp->getUuid(), {20.f, 4.f, 0.f}, scene->getSceneId());
    ASSERT_TRUE(second) << second.status.msg();
    EXPECT_EQ(session->view().undoCount, 1u);

    ASSERT_TRUE(session->undo());
    EXPECT_EQ(comp->getTransform().position, start);
    ASSERT_TRUE(session->redo());
    EXPECT_EQ(comp->getTransform().position, glm::vec3(20.f, 4.f, 0.f));
}

TEST_F(ProjectSessionTest, EditMergeDoesNotCrossSavePoint) {
    TmpDir tmp;
    auto comp = std::make_shared<Bess::Canvas::SceneComponent>();
    ASSERT_TRUE(session->addComp(comp, {}, scene->getSceneId()));
    session->clearHist();

    ASSERT_TRUE(session->moveComp(
        comp->getUuid(), {10.f, 0.f, 0.f}, scene->getSceneId()));
    ASSERT_TRUE(session->saveAs(tmp.path / "saved.bproj"));
    ASSERT_FALSE(session->dirty());

    ASSERT_TRUE(session->moveComp(
        comp->getUuid(), {20.f, 0.f, 0.f}, scene->getSceneId()));
    EXPECT_EQ(session->view().undoCount, 2u);
    EXPECT_TRUE(session->dirty());

    ASSERT_TRUE(session->undo());
    EXPECT_EQ(comp->getTransform().position, glm::vec3(10.f, 0.f, 0.f));
    EXPECT_FALSE(session->dirty());
}

TEST_F(ProjectSessionTest, FailedMultiStepEditRollsBackAtomically) {
    auto comp = std::make_shared<Bess::Canvas::SceneComponent>();
    auto tx = session->tx("Atomic edit");
    ASSERT_TRUE(tx.addComp(comp, {}, scene->getSceneId()));
    ASSERT_TRUE(tx.step(
        "Fail",
        [](Bess::ProjectSession &) {
            return Bess::Status::fail(Bess::Err::apply, "expected failure");
        },
        [](Bess::ProjectSession &) { return Bess::Status::ok(); }));

    const auto result = tx.commit();
    EXPECT_FALSE(result);
    EXPECT_EQ(result.status.err(), Bess::Err::apply);
    EXPECT_TRUE(scene->getState().getAllComponents().empty());
    EXPECT_FALSE(session->canUndo());
    EXPECT_FALSE(session->faulted());
}

TEST_F(ProjectSessionTest, StagingErrorPreventsPartialCommit) {
    auto comp = std::make_shared<Bess::Canvas::SceneComponent>();
    auto tx = session->tx("Invalid edit");
    EXPECT_FALSE(tx.addComp(nullptr, {}, scene->getSceneId()));
    EXPECT_TRUE(tx.addComp(comp, {}, scene->getSceneId()));

    const auto result = tx.commit();
    EXPECT_FALSE(result);
    EXPECT_EQ(result.status.err(), Bess::Err::badArg);
    EXPECT_FALSE(scene->getState().isComponentValid(comp->getUuid()));
    EXPECT_FALSE(session->canUndo());
}

TEST_F(ProjectSessionTest, SaveLoadRestoresDocAndSavePoint) {
    TmpDir tmp;
    const auto path = tmp.path / "project.bproj";

    ASSERT_TRUE(session->newProj("Alpha"));
    ASSERT_TRUE(session->saveAs(path));
    EXPECT_FALSE(session->dirty());
    EXPECT_EQ(session->doc().path(), std::filesystem::absolute(path));

    ASSERT_TRUE(session->setName("Beta"));
    EXPECT_TRUE(session->dirty());
    EXPECT_EQ(session->doc().name(), "Beta");

    ASSERT_TRUE(session->load(path));
    EXPECT_EQ(session->doc().name(), "Alpha");
    EXPECT_FALSE(session->dirty());
    EXPECT_FALSE(session->canUndo());
    EXPECT_FALSE(session->canRedo());
}

TEST_F(ProjectSessionTest, BadLoadLeavesCurrentProjectUntouched) {
    TmpDir tmp;
    const auto saved = tmp.path / "saved.bproj";
    const auto bad = tmp.path / "bad.bproj";

    ASSERT_TRUE(session->newProj("Stable"));
    ASSERT_TRUE(session->saveAs(saved));
    const auto root = session->scenes().getRootSceneId();
    const auto count = session->scenes().getSceneCount();

    {
        std::ofstream out(bad, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out);
        out << "{ definitely not json";
    }

    const auto result = session->load(bad);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.err(), Bess::Err::parse);
    EXPECT_EQ(session->doc().name(), "Stable");
    EXPECT_EQ(session->doc().path(), std::filesystem::absolute(saved));
    EXPECT_EQ(session->scenes().getRootSceneId(), root);
    EXPECT_EQ(session->scenes().getSceneCount(), count);
    EXPECT_FALSE(session->dirty());
}

TEST_F(ProjectSessionTest, UnknownSimDefRollsBackLoad) {
    TmpDir tmp;
    const auto saved = tmp.path / "saved.bproj";
    const auto bad = tmp.path / "unknown-def.bproj";

    ASSERT_TRUE(session->newProj("Stable"));
    ASSERT_TRUE(session->saveAs(saved));
    const auto root = session->scenes().getRootSceneId();
    const auto count = session->scenes().getSceneCount();

    auto data = session->doc().json();
    Json::Value comp{Json::objectValue};
    comp["def"]["name"] = "missing";
    comp["def"]["typeName"] = "missing";
    data["sim_engine_data"]["drivers"]["digitalsimdriver"]["components"].append(
        std::move(comp));

    {
        std::ofstream out(bad, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out);
        out << data;
    }

    const auto result = session->load(bad);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.err(), Bess::Err::apply);
    EXPECT_EQ(session->doc().name(), "Stable");
    EXPECT_EQ(session->doc().path(), std::filesystem::absolute(saved));
    EXPECT_EQ(session->scenes().getRootSceneId(), root);
    EXPECT_EQ(session->scenes().getSceneCount(), count);
    EXPECT_FALSE(session->dirty());
    EXPECT_FALSE(session->faulted());
}

TEST_F(ProjectSessionTest, UiRemovalStillRecursesThroughUiChildren) {
    auto parent = Bess::Canvas::UI::ContainerComp::create();
    auto child = Bess::Canvas::UI::LabelComp::create("child");

    auto &state = scene->getState();
    state.addComponent(parent, false, false);
    state.addComponent(child, false, false);
    state.attachChild(parent->getUuid(), child->getUuid(), false);
    state.removeComponent(parent->getUuid());

    EXPECT_FALSE(state.isComponentValid(parent->getUuid()));
    EXPECT_FALSE(state.isComponentValid(child->getUuid()));
}
