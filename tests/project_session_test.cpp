#include "project_session/json_project_store.h"
#include "project_session/project_session.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace Bess::Session {
    namespace {
        class TestRestorePoint final : public SimulationRestorePoint {
          public:
            explicit TestRestorePoint(SimulationComponentInfo info)
                : m_info(std::move(info)) {
            }

            UUID id() const noexcept override {
                return m_info.id;
            }

            const SimulationComponentInfo &info() const {
                return m_info;
            }

          private:
            SimulationComponentInfo m_info;
        };

        class TestSimulationGateway final : public ISimulationGateway {
          public:
            TestSimulationGateway() {
                SimulationComponentInfo logic;
                logic.definition.name = "Logic";
                logic.inputs = {
                    .direction = SimEngine::PortDirection::input,
                    .signalKind = SimEngine::SignalKind::digital,
                    .count = 2,
                    .names = {"A", "B"},
                    .isResizeable = true,
                };
                logic.outputs = {
                    .direction = SimEngine::PortDirection::output,
                    .signalKind = SimEngine::SignalKind::digital,
                    .count = 1,
                    .names = {"Q"},
                    .isResizeable = true,
                };
                m_definitions.emplace("Logic", std::move(logic));
            }

            Result<SimulationComponentInfo>
            createComponent(const SimEngine::CompDefRef &definition) override {
                const auto found = m_definitions.find(definition.name);
                if (found == m_definitions.end()) {
                    return fail(Error::notFound("Definition not found"));
                }
                auto info = found->second;
                info.id = UUID{};
                info.definition = definition;
                m_components.emplace(info.id, info);
                return info;
            }

            Result<std::unique_ptr<SimulationRestorePoint>>
            removeComponent(UUID id) override {
                const auto found = m_components.find(id);
                if (found == m_components.end()) {
                    return fail(Error::notFound("Component not found"));
                }
                auto restorePoint =
                    std::make_unique<TestRestorePoint>(found->second);
                m_components.erase(found);
                std::erase_if(m_connections, [id](const auto &connection) {
                    return connection.first.componentId == id ||
                           connection.second.componentId == id;
                });
                return std::unique_ptr<SimulationRestorePoint>(
                    std::move(restorePoint));
            }

            Result<SimulationComponentInfo> restoreComponent(
                const SimulationRestorePoint &restorePoint) override {
                const auto *testPoint =
                    dynamic_cast<const TestRestorePoint *>(&restorePoint);
                if (!testPoint) {
                    return fail(Error::invalidArgument(
                        "Unexpected restore-point type"));
                }
                m_components[testPoint->id()] = testPoint->info();
                return testPoint->info();
            }

            Result<SimulationComponentInfo>
            componentInfo(UUID id) const override {
                const auto found = m_components.find(id);
                if (found == m_components.end()) {
                    return fail(Error::notFound("Component not found"));
                }
                return found->second;
            }

            Status connect(const SimEngine::PortRef &source,
                           const SimEngine::PortRef &destination) override {
                if (!m_components.contains(source.componentId) ||
                    !m_components.contains(destination.componentId)) {
                    return fail(Error::notFound("Endpoint not found"));
                }
                m_connections.emplace_back(source, destination);
                return {};
            }

            Status disconnect(const SimEngine::PortRef &source,
                              const SimEngine::PortRef &destination) override {
                const auto found = std::find_if(
                    m_connections.begin(),
                    m_connections.end(),
                    [&](const auto &candidate) {
                        return samePort(candidate.first, source) &&
                               samePort(candidate.second, destination);
                    });
                if (found != m_connections.end()) {
                    m_connections.erase(found);
                }
                return {};
            }

            Result<SimulationComponentInfo>
            addPort(const SimEngine::PortRef &port) override {
                auto found = m_components.find(port.componentId);
                if (found == m_components.end()) {
                    return fail(Error::notFound("Component not found"));
                }
                auto &descriptor =
                    port.direction == SimEngine::PortDirection::input
                        ? found->second.inputs
                        : found->second.outputs;
                ++descriptor.count;
                return found->second;
            }

            Result<SimulationComponentInfo>
            removePort(const SimEngine::PortRef &port) override {
                auto found = m_components.find(port.componentId);
                if (found == m_components.end()) {
                    return fail(Error::notFound("Component not found"));
                }
                auto &descriptor =
                    port.direction == SimEngine::PortDirection::input
                        ? found->second.inputs
                        : found->second.outputs;
                if (descriptor.count == 0) {
                    return fail(Error::conflict("No ports remain"));
                }
                --descriptor.count;
                return found->second;
            }

            Result<SimEngine::PortState>
            portState(const SimEngine::PortRef &) const override {
                return SimEngine::PortState::digital(
                    SimEngine::LogicState::low);
            }

            SimEngine::SimulationState simulationState() const override {
                return m_state;
            }

            Status
            setSimulationState(SimEngine::SimulationState state) override {
                m_state = state;
                return {};
            }

            Status stepSimulation() override {
                return m_state == SimEngine::SimulationState::paused
                           ? Status{}
                           : fail(Error::conflict("Not paused"));
            }

            Status reset() override {
                m_components.clear();
                m_connections.clear();
                m_state = SimEngine::SimulationState::stopped;
                return {};
            }

            std::size_t componentCount() const {
                return m_components.size();
            }

            std::size_t connectionCount() const {
                return m_connections.size();
            }

          private:
            static bool samePort(const SimEngine::PortRef &a,
                                 const SimEngine::PortRef &b) {
                return a.componentId == b.componentId &&
                       a.direction == b.direction &&
                       a.signalKind == b.signalKind && a.index == b.index;
            }

          private:
            std::map<std::string, SimulationComponentInfo> m_definitions;
            std::unordered_map<UUID, SimulationComponentInfo> m_components;
            std::vector<std::pair<SimEngine::PortRef, SimEngine::PortRef>>
                m_connections;
            SimEngine::SimulationState m_state =
                SimEngine::SimulationState::stopped;
        };

        std::vector<UUID> ports(const SceneDocument &scene,
                                UUID parent,
                                SimEngine::PortDirection direction) {
            std::vector<UUID> result;
            const auto *hierarchy = scene.tryGet<HierarchyComponent>(parent);
            if (!hierarchy) {
                return result;
            }
            for (const auto child : hierarchy->children) {
                const auto *port = scene.tryGet<PortComponent>(child);
                if (port && !port->resizeTrigger &&
                    port->direction == direction) {
                    result.push_back(child);
                }
            }
            std::sort(result.begin(), result.end(), [&](UUID a, UUID b) {
                return scene.tryGet<PortComponent>(a)->index <
                       scene.tryGet<PortComponent>(b)->index;
            });
            return result;
        }
    } // namespace

    TEST(ProjectSessionSceneDocument, MaintainsHierarchyAndRuntimeState) {
        SceneDocument scene({.id = UUID{}, .name = "Test"});

        EntityRecord parent;
        parent.identity.id = UUID{};
        parent.identity.name = "Parent";
        parent.interaction.focusable = true;

        EntityRecord child;
        child.identity.id = UUID{};
        child.identity.name = "Child";
        child.hierarchy.parent = parent.identity.id;

        ASSERT_TRUE(scene.createEntity(parent));
        ASSERT_TRUE(scene.createEntity(child));
        EXPECT_FALSE(scene.setParent(parent.identity.id, child.identity.id));
        ASSERT_TRUE(scene.select(child.identity.id));
        ASSERT_TRUE(scene.focus(parent.identity.id));

        const auto picking = scene.pickingId(child.identity.id);
        ASSERT_TRUE(picking);
        EXPECT_EQ(scene.entityFromPickingId(*picking), child.identity.id);
        ASSERT_TRUE(scene.validate());

        auto mutation = scene.destroyEntity(parent.identity.id);
        ASSERT_TRUE(mutation);
        EXPECT_EQ(scene.size(), 0U);
        ASSERT_TRUE(scene.restoreMutation(*mutation));
        EXPECT_EQ(scene.size(), 2U);
        ASSERT_TRUE(scene.validate());
        EXPECT_EQ(scene.tryGet<HierarchyComponent>(child.identity.id)->parent,
                  parent.identity.id);
    }

    TEST(ProjectSessionHistory, TracksDirtyStateAcrossUndoAndRedo) {
        ProjectSession session;
        UUID entity = UUID::null;
        const auto initialRevision = session.revision();
        EXPECT_TRUE(session.isDirty());

        ASSERT_TRUE(session.transact(
            [&](TransactionCtx &transaction) {
                EntityRecord record;
                record.identity.name = "Note";
                record.identity.kind = EntityKind::text;
                record.text = TextComponent{.text = "hello"};
                entity = transaction.addEntity(std::move(record));
            },
            "Add note"));

        EXPECT_NE(entity, UUID::null);
        EXPECT_TRUE(session.isDirty());
        EXPECT_EQ(session.nextUndoName(), "Add note");
        EXPECT_GT(session.revision(), initialRevision);
        {
            const auto project = session.read();
            EXPECT_TRUE(project->activeScene()->contains(entity));
        }

        ASSERT_TRUE(session.undo());
        EXPECT_EQ(session.revision(), initialRevision);
        {
            const auto project = session.read();
            EXPECT_FALSE(project->activeScene()->contains(entity));
        }

        ASSERT_TRUE(session.redo());
        {
            const auto project = session.read();
            EXPECT_TRUE(project->activeScene()->contains(entity));
        }
    }

    TEST(ProjectSessionSimulation,
         KeepsSceneAndSimulationGraphAtomicAcrossHistory) {
        auto gateway = std::make_shared<TestSimulationGateway>();
        ProjectSession session(gateway);
        UUID first = UUID::null;
        UUID second = UUID::null;

        ASSERT_TRUE(session.transact(
            [&](TransactionCtx &transaction) {
                first = transaction.addComp({.name = "Logic"});
                second = transaction.addComp({.name = "Logic"});
            },
            "Add logic"));
        EXPECT_EQ(gateway->componentCount(), 2U);

        UUID output = UUID::null;
        UUID input = UUID::null;
        {
            const auto project = session.read();
            const auto &scene = *project->activeScene();
            output =
                ports(scene, first, SimEngine::PortDirection::output).front();
            input =
                ports(scene, second, SimEngine::PortDirection::input).front();
        }

        UUID connection = UUID::null;
        ASSERT_TRUE(session.transact(
            [&](TransactionCtx &transaction) {
                connection = transaction.addConnection(output, input);
            },
            "Connect logic"));
        EXPECT_EQ(gateway->connectionCount(), 1U);

        ASSERT_TRUE(session.transact(
            [&](TransactionCtx &transaction) {
                EXPECT_TRUE(transaction.removeComp(first));
            },
            "Remove logic"));
        EXPECT_EQ(gateway->componentCount(), 1U);
        EXPECT_EQ(gateway->connectionCount(), 0U);
        {
            const auto project = session.read();
            EXPECT_FALSE(project->activeScene()->contains(first));
            EXPECT_FALSE(project->activeScene()->contains(connection));
        }

        ASSERT_TRUE(session.undo());
        EXPECT_EQ(gateway->componentCount(), 2U);
        EXPECT_EQ(gateway->connectionCount(), 1U);
        {
            const auto project = session.read();
            EXPECT_TRUE(project->activeScene()->contains(first));
            EXPECT_TRUE(project->activeScene()->contains(connection));
            EXPECT_TRUE(project->validate());
        }
    }

    TEST(ProjectSessionPersistence, RoundTripsVersionedProjectData) {
        ProjectSession session;
        UUID entity = UUID::null;
        ASSERT_TRUE(session.transact([&](TransactionCtx &transaction) {
            EntityRecord record;
            record.identity.name = "Annotation";
            record.identity.kind = EntityKind::text;
            record.transform.position = {12.0F, 24.0F, 0.5F};
            record.text = TextComponent{
                .text = "hello",
                .foregroundColor = {0.1F, 0.2F, 0.3F, 1.0F},
                .fontSize = 18,
            };
            record.image = ImageComponent{
                .rgba = {1, 2, 3, 255},
                .width = 1,
                .height = 1,
            };
            record.extensions = ExtensionComponents{
                .values = {{"example.plugin", Json::Value("payload")}},
            };
            entity = transaction.addEntity(std::move(record));
        }));

        JsonProjectStore store;
        Json::Value encoded;
        {
            const auto project = session.read();
            auto result = store.encode(*project);
            ASSERT_TRUE(result) << result.error().message;
            encoded = std::move(*result);
        }

        auto decoded = store.decode(encoded);
        ASSERT_TRUE(decoded) << decoded.error().message;
        ASSERT_TRUE((*decoded)->validate());
        const auto *decodedEntity =
            (*decoded)->activeScene()->tryGet<TextComponent>(entity);
        ASSERT_NE(decodedEntity, nullptr);
        EXPECT_EQ(decodedEntity->text, "hello");
        EXPECT_EQ(decodedEntity->fontSize, 18U);
        const auto *decodedImage =
            (*decoded)->activeScene()->tryGet<ImageComponent>(entity);
        ASSERT_NE(decodedImage, nullptr);
        EXPECT_EQ(decodedImage->rgba, (std::vector<uint8_t>{1, 2, 3, 255}));
        const auto *decodedExtensions =
            (*decoded)->activeScene()->tryGet<ExtensionComponents>(entity);
        ASSERT_NE(decodedExtensions, nullptr);
        EXPECT_EQ(decodedExtensions->values.at("example.plugin").asString(),
                  "payload");

        const auto directory = std::filesystem::temp_directory_path() /
                               ("bess-project-session-" + UUID{}.toString());
        ASSERT_TRUE(std::filesystem::create_directory(directory));
        const auto path = directory / "roundtrip.bproj";
        ASSERT_TRUE(store.save(path, **decoded));
        auto loaded = store.load(path);
        EXPECT_TRUE(loaded) << loaded.error().message;
        std::error_code cleanupError;
        std::filesystem::remove_all(directory, cleanupError);
        EXPECT_FALSE(cleanupError);
    }
} // namespace Bess::Session
