#include "project_session/transaction.h"

#include <algorithm>
#include <format>
#include <unordered_set>

namespace Bess::Session {
    namespace {
        SceneDocument *requireScene(ProjectDocument &document, UUID id) {
            return document.scene(id);
        }

        const SceneDocument *requireScene(const ProjectDocument &document,
                                          UUID id) {
            return document.scene(id);
        }

        void recordChange(TxnExecCtx &context,
                          ChangeKind kind,
                          UUID scene,
                          UUID entity,
                          std::string detail = {}) {
            context.changes.push_back({kind, scene, entity, std::move(detail)});
        }

        Result<SimEngine::PortRef>
        resolveEnginePort(const SceneDocument &scene,
                          UUID endpoint,
                          SimEngine::PortDirection requiredDirection) {
            const auto *proxy = scene.tryGet<ProxyPortComponent>(endpoint);
            if (proxy) {
                endpoint = requiredDirection == SimEngine::PortDirection::output
                               ? proxy->outputPort
                               : proxy->inputPort;
            }

            const auto *port = scene.tryGet<PortComponent>(endpoint);
            const auto *hierarchy = scene.tryGet<HierarchyComponent>(endpoint);
            if (!port || !hierarchy || hierarchy->parent == UUID::null) {
                return fail(Error::invalidArgument(
                    "Connection endpoint is not a simulation port"));
            }
            if (port->resizeTrigger || port->index < 0) {
                return fail(Error::conflict(
                    "Resize controls cannot be used as connection endpoints"));
            }
            if (port->direction != requiredDirection) {
                return fail(Error::conflict(
                    requiredDirection == SimEngine::PortDirection::output
                        ? "Connection source must be an output port"
                        : "Connection destination must be an input port"));
            }

            const auto *simulation =
                scene.tryGet<SimulationComponent>(hierarchy->parent);
            if (!simulation || simulation->simulationId == UUID::null) {
                return fail(Error::invalidState(
                    "Port owner has no simulation engine component"));
            }

            return SimEngine::PortRef{
                .componentId = simulation->simulationId,
                .direction = port->direction,
                .signalKind = port->signalKind,
                .index = port->index,
            };
        }

        EntityRecord makePortRecord(UUID id,
                                    UUID parent,
                                    const SimEngine::PortDescriptor &descriptor,
                                    int index,
                                    std::string name,
                                    bool resizeTrigger = false) {
            EntityRecord record;
            record.identity.id = id;
            record.identity.name = std::move(name);
            record.identity.kind = EntityKind::port;
            record.identity.showInProjectExplorer = false;
            record.hierarchy.parent = parent;
            record.interaction.selectable = !resizeTrigger;
            record.port = PortComponent{
                .direction = descriptor.direction,
                .signalKind = descriptor.signalKind,
                .index = index,
                .resizeTrigger = resizeTrigger,
            };
            return record;
        }

        std::string portName(const SimEngine::PortDescriptor &descriptor,
                             std::size_t index) {
            if (index < descriptor.names.size() &&
                !descriptor.names[index].empty()) {
                return descriptor.names[index];
            }
            const char prefix =
                descriptor.direction == SimEngine::PortDirection::input ? 'A'
                                                                        : 'a';
            if (index < 26) {
                return std::string(1, static_cast<char>(prefix + index));
            }
            return std::format("{}{}", prefix, index);
        }

        class AddSceneOperation final : public TransactionOperation {
          public:
            AddSceneOperation(UUID scene, std::string name)
                : m_scene(scene),
                  m_name(std::move(name)) {
            }

            std::string name() const override {
                return "Add scene";
            }

            Status execute(TxnExecCtx &context) override {
                auto scene =
                    context.document.createScene(m_name, true, m_scene);
                if (!scene) {
                    return fail(std::move(scene.error()));
                }
                recordChange(
                    context, ChangeKind::sceneAdded, m_scene, UUID::null);
                recordChange(context,
                             ChangeKind::activeSceneChanged,
                             m_scene,
                             UUID::null);
                return {};
            }

            Status undo(TxnExecCtx &context) override {
                m_index = context.document.sceneIndex(m_scene);
                auto removed = context.document.removeScene(m_scene);
                if (!removed) {
                    return fail(std::move(removed.error()));
                }
                m_removed = std::move(*removed);
                recordChange(
                    context, ChangeKind::sceneRemoved, m_scene, UUID::null);
                return {};
            }

            Status redo(TxnExecCtx &context) override {
                if (!m_removed) {
                    return execute(context);
                }
                if (auto status = context.document.restoreScene(
                        std::move(m_removed), m_index);
                    !status) {
                    return status;
                }
                if (auto status = context.document.setActiveScene(m_scene);
                    !status) {
                    return status;
                }
                recordChange(
                    context, ChangeKind::sceneAdded, m_scene, UUID::null);
                recordChange(context,
                             ChangeKind::activeSceneChanged,
                             m_scene,
                             UUID::null);
                return {};
            }

          private:
            UUID m_scene;
            std::string m_name;
            std::size_t m_index = 0;
            std::unique_ptr<SceneDocument> m_removed;
        };

        class RemoveSceneOperation final : public TransactionOperation {
          public:
            explicit RemoveSceneOperation(UUID scene) : m_scene(scene) {
            }

            std::string name() const override {
                return "Remove scene";
            }

            Status execute(TxnExecCtx &context) override {
                m_wasActive = context.document.activeSceneId() == m_scene;
                m_index = context.document.sceneIndex(m_scene);
                auto removed = context.document.removeScene(m_scene);
                if (!removed) {
                    return fail(std::move(removed.error()));
                }
                m_removed = std::move(*removed);
                recordChange(
                    context, ChangeKind::sceneRemoved, m_scene, UUID::null);
                if (m_wasActive) {
                    recordChange(context,
                                 ChangeKind::activeSceneChanged,
                                 context.document.activeSceneId(),
                                 UUID::null);
                }
                return {};
            }

            Status undo(TxnExecCtx &context) override {
                if (!m_removed) {
                    return fail(Error::invalidState(
                        "Removed scene has no restorable state"));
                }
                if (auto status = context.document.restoreScene(
                        std::move(m_removed), m_index);
                    !status) {
                    return status;
                }
                if (m_wasActive) {
                    if (auto status = context.document.setActiveScene(m_scene);
                        !status) {
                        return status;
                    }
                }
                recordChange(
                    context, ChangeKind::sceneAdded, m_scene, UUID::null);
                return {};
            }

            Status redo(TxnExecCtx &context) override {
                return execute(context);
            }

          private:
            UUID m_scene;
            std::size_t m_index = 0;
            bool m_wasActive = false;
            std::unique_ptr<SceneDocument> m_removed;
        };

        class SetActiveSceneOperation final : public TransactionOperation {
          public:
            explicit SetActiveSceneOperation(UUID scene) : m_scene(scene) {
            }

            std::string name() const override {
                return "Set active scene";
            }

            Status execute(TxnExecCtx &context) override {
                m_previous = context.document.activeSceneId();
                if (auto status = context.document.setActiveScene(m_scene);
                    !status) {
                    return status;
                }
                recordChange(context,
                             ChangeKind::activeSceneChanged,
                             m_scene,
                             UUID::null);
                return {};
            }

            Status undo(TxnExecCtx &context) override {
                if (auto status = context.document.setActiveScene(m_previous);
                    !status) {
                    return status;
                }
                recordChange(context,
                             ChangeKind::activeSceneChanged,
                             m_previous,
                             UUID::null);
                return {};
            }

            Status redo(TxnExecCtx &context) override {
                if (auto status = context.document.setActiveScene(m_scene);
                    !status) {
                    return status;
                }
                recordChange(context,
                             ChangeKind::activeSceneChanged,
                             m_scene,
                             UUID::null);
                return {};
            }

          private:
            UUID m_scene;
            UUID m_previous = UUID::null;
        };

        class AddEntityOperation final : public TransactionOperation {
          public:
            AddEntityOperation(UUID scene, EntityRecord record)
                : m_scene(scene),
                  m_record(std::move(record)) {
            }

            std::string name() const override {
                return "Add entity";
            }

            Status execute(TxnExecCtx &context) override {
                auto *scene = requireScene(context.document, m_scene);
                if (!scene) {
                    return fail(Error::notFound("Scene does not exist"));
                }
                auto created = scene->createEntity(m_record);
                if (!created) {
                    return fail(std::move(created.error()));
                }
                recordChange(
                    context, ChangeKind::entityAdded, m_scene, *created);
                return {};
            }

            Status undo(TxnExecCtx &context) override {
                auto *scene = requireScene(context.document, m_scene);
                if (!scene) {
                    return fail(Error::notFound("Scene does not exist"));
                }
                auto mutation = scene->destroyEntity(m_record.identity.id);
                if (!mutation) {
                    return fail(std::move(mutation.error()));
                }
                m_undoMutation = std::move(*mutation);
                recordChange(context,
                             ChangeKind::entityRemoved,
                             m_scene,
                             m_record.identity.id);
                return {};
            }

            Status redo(TxnExecCtx &context) override {
                auto *scene = requireScene(context.document, m_scene);
                if (!scene) {
                    return fail(Error::notFound("Scene does not exist"));
                }
                if (m_undoMutation) {
                    if (auto status = scene->restoreMutation(*m_undoMutation);
                        !status) {
                        return status;
                    }
                } else {
                    auto result = scene->createEntity(m_record);
                    if (!result) {
                        return fail(std::move(result.error()));
                    }
                }
                recordChange(context,
                             ChangeKind::entityAdded,
                             m_scene,
                             m_record.identity.id);
                return {};
            }

          private:
            UUID m_scene;
            EntityRecord m_record;
            std::optional<SceneMutation> m_undoMutation;
        };

        class RemoveEntityOperation final : public TransactionOperation {
          public:
            RemoveEntityOperation(UUID scene, UUID entity)
                : m_scene(scene),
                  m_entity(entity) {
            }

            std::string name() const override {
                return "Remove entity";
            }

            Status execute(TxnExecCtx &context) override {
                auto *scene = requireScene(context.document, m_scene);
                if (!scene) {
                    return fail(Error::notFound("Scene does not exist"));
                }
                if (scene->tryGet<SimulationComponent>(m_entity)) {
                    return fail(Error::conflict(
                        "Simulation entities must be removed with removeComp"));
                }
                auto mutation = scene->destroyEntity(m_entity);
                if (!mutation) {
                    return fail(std::move(mutation.error()));
                }
                m_mutation = std::move(*mutation);
                recordChange(
                    context, ChangeKind::entityRemoved, m_scene, m_entity);
                return {};
            }

            Status undo(TxnExecCtx &context) override {
                auto *scene = requireScene(context.document, m_scene);
                if (!scene || !m_mutation) {
                    return fail(Error::invalidState(
                        "Removed entity has no restorable state"));
                }
                if (auto status = scene->restoreMutation(*m_mutation);
                    !status) {
                    return status;
                }
                recordChange(
                    context, ChangeKind::entityAdded, m_scene, m_entity);
                return {};
            }

            Status redo(TxnExecCtx &context) override {
                return execute(context);
            }

          private:
            UUID m_scene;
            UUID m_entity;
            std::optional<SceneMutation> m_mutation;
        };

        class AddSimulationOperation final : public TransactionOperation {
          public:
            AddSimulationOperation(UUID scene,
                                   UUID entity,
                                   SimEngine::CompDefRef definition,
                                   TransformComponent transform)
                : m_scene(scene),
                  m_entity(entity),
                  m_definition(std::move(definition)),
                  m_transform(transform) {
            }

            std::string name() const override {
                return "Add simulation component";
            }

            Status execute(TxnExecCtx &context) override {
                auto *scene = requireScene(context.document, m_scene);
                if (!scene) {
                    return fail(Error::notFound("Scene does not exist"));
                }

                auto info = context.simulation.createComponent(m_definition);
                if (!info) {
                    return fail(std::move(info.error()));
                }
                if (auto status = createSceneRecords(*scene, *info); !status) {
                    auto rollback =
                        context.simulation.removeComponent(info->id);
                    if (!rollback) {
                        return fail(Error::rollbackFailure(
                            status.error().message +
                            "; simulation rollback also failed: " +
                            rollback.error().message));
                    }
                    return status;
                }

                recordChange(
                    context, ChangeKind::entityAdded, m_scene, m_entity);
                return {};
            }

            Status undo(TxnExecCtx &context) override {
                auto *scene = requireScene(context.document, m_scene);
                if (!scene) {
                    return fail(Error::notFound("Scene does not exist"));
                }
                const auto *simulation =
                    scene->tryGet<SimulationComponent>(m_entity);
                if (!simulation) {
                    return fail(Error::invalidState(
                        "Simulation scene entity is missing"));
                }
                const auto simulationId = simulation->simulationId;

                auto mutation = scene->destroyEntity(m_entity);
                if (!mutation) {
                    return fail(std::move(mutation.error()));
                }
                auto restorePoint =
                    context.simulation.removeComponent(simulationId);
                if (!restorePoint) {
                    auto rollback = scene->restoreMutation(*mutation);
                    if (!rollback) {
                        return fail(Error::rollbackFailure(
                            restorePoint.error().message +
                            "; scene rollback also failed: " +
                            rollback.error().message));
                    }
                    return fail(std::move(restorePoint.error()));
                }

                m_mutation = std::move(*mutation);
                m_restorePoint = std::move(*restorePoint);
                recordChange(
                    context, ChangeKind::entityRemoved, m_scene, m_entity);
                return {};
            }

            Status redo(TxnExecCtx &context) override {
                auto *scene = requireScene(context.document, m_scene);
                if (!scene || !m_mutation || !m_restorePoint) {
                    return fail(Error::invalidState(
                        "Added simulation component has no redo state"));
                }

                auto restored =
                    context.simulation.restoreComponent(*m_restorePoint);
                if (!restored) {
                    return fail(std::move(restored.error()));
                }
                if (auto status = scene->restoreMutation(*m_mutation);
                    !status) {
                    auto rollback =
                        context.simulation.removeComponent(restored->id);
                    if (!rollback) {
                        return fail(Error::rollbackFailure(
                            status.error().message +
                            "; simulation rollback also failed: " +
                            rollback.error().message));
                    }
                    return status;
                }
                recordChange(
                    context, ChangeKind::entityAdded, m_scene, m_entity);
                return {};
            }

          private:
            Status createSceneRecords(SceneDocument &scene,
                                      const SimulationComponentInfo &info) {
                EntityRecord node;
                node.identity.id = m_entity;
                node.identity.name = m_definition.name;
                node.identity.kind = EntityKind::simulation;
                node.transform = m_transform;
                node.interaction.draggable = true;
                node.simulation = SimulationComponent{
                    .simulationId = info.id,
                    .definition = m_definition,
                };

                auto created = scene.createEntity(std::move(node));
                if (!created) {
                    return fail(std::move(created.error()));
                }

                std::vector<UUID> createdPorts;
                auto createPorts =
                    [&](const SimEngine::PortDescriptor &descriptor,
                        std::vector<UUID> &ids,
                        std::optional<UUID> &resizeId) -> Status {
                    if (ids.empty()) {
                        ids.reserve(descriptor.count);
                        for (std::size_t index = 0; index < descriptor.count;
                             ++index) {
                            ids.emplace_back(UUID{});
                        }
                    }
                    if (ids.size() != descriptor.count) {
                        return fail(Error::invalidState(
                            "Simulation port count changed across redo"));
                    }

                    for (std::size_t index = 0; index < descriptor.count;
                         ++index) {
                        auto port = makePortRecord(ids[index],
                                                   m_entity,
                                                   descriptor,
                                                   static_cast<int>(index),
                                                   portName(descriptor, index));
                        auto result = scene.createEntity(std::move(port));
                        if (!result) {
                            return fail(std::move(result.error()));
                        }
                        createdPorts.push_back(*result);
                    }

                    if (descriptor.isResizeable) {
                        if (!resizeId) {
                            resizeId = UUID{};
                        }
                        auto resize = makePortRecord(
                            *resizeId,
                            m_entity,
                            descriptor,
                            -1,
                            descriptor.direction ==
                                    SimEngine::PortDirection::input
                                ? "Add input"
                                : "Add output",
                            true);
                        auto result = scene.createEntity(std::move(resize));
                        if (!result) {
                            return fail(std::move(result.error()));
                        }
                        createdPorts.push_back(*result);
                    }
                    return {};
                };

                if (auto status = createPorts(
                        info.inputs, m_inputPorts, m_inputResizePort);
                    !status) {
                    auto rollback = scene.destroyEntity(m_entity);
                    (void)rollback;
                    return status;
                }
                if (auto status = createPorts(
                        info.outputs, m_outputPorts, m_outputResizePort);
                    !status) {
                    auto rollback = scene.destroyEntity(m_entity);
                    (void)rollback;
                    return status;
                }
                return {};
            }

          private:
            UUID m_scene;
            UUID m_entity;
            SimEngine::CompDefRef m_definition;
            TransformComponent m_transform;
            std::vector<UUID> m_inputPorts;
            std::vector<UUID> m_outputPorts;
            std::optional<UUID> m_inputResizePort;
            std::optional<UUID> m_outputResizePort;
            std::optional<SceneMutation> m_mutation;
            std::unique_ptr<SimulationRestorePoint> m_restorePoint;
        };

        class RemoveSimulationOperation final : public TransactionOperation {
          public:
            RemoveSimulationOperation(UUID scene, UUID entity)
                : m_scene(scene),
                  m_entity(entity) {
            }

            std::string name() const override {
                return "Remove simulation component";
            }

            Status execute(TxnExecCtx &context) override {
                auto *scene = requireScene(context.document, m_scene);
                if (!scene) {
                    return fail(Error::notFound("Scene does not exist"));
                }
                const auto *simulation =
                    scene->tryGet<SimulationComponent>(m_entity);
                if (!simulation) {
                    return fail(Error::invalidArgument(
                        "Entity is not a simulation component"));
                }
                const auto simulationId = simulation->simulationId;

                auto mutation = scene->destroyEntity(m_entity);
                if (!mutation) {
                    return fail(std::move(mutation.error()));
                }
                auto restorePoint =
                    context.simulation.removeComponent(simulationId);
                if (!restorePoint) {
                    auto rollback = scene->restoreMutation(*mutation);
                    if (!rollback) {
                        return fail(Error::rollbackFailure(
                            restorePoint.error().message +
                            "; scene rollback also failed: " +
                            rollback.error().message));
                    }
                    return fail(std::move(restorePoint.error()));
                }

                m_mutation = std::move(*mutation);
                m_restorePoint = std::move(*restorePoint);
                recordChange(
                    context, ChangeKind::entityRemoved, m_scene, m_entity);
                return {};
            }

            Status undo(TxnExecCtx &context) override {
                auto *scene = requireScene(context.document, m_scene);
                if (!scene || !m_mutation || !m_restorePoint) {
                    return fail(Error::invalidState(
                        "Removed component has no restorable state"));
                }

                auto restored =
                    context.simulation.restoreComponent(*m_restorePoint);
                if (!restored) {
                    return fail(std::move(restored.error()));
                }
                if (auto status = scene->restoreMutation(*m_mutation);
                    !status) {
                    auto rollback =
                        context.simulation.removeComponent(restored->id);
                    if (!rollback) {
                        return fail(Error::rollbackFailure(
                            status.error().message +
                            "; simulation rollback also failed: " +
                            rollback.error().message));
                    }
                    return status;
                }

                recordChange(
                    context, ChangeKind::entityAdded, m_scene, m_entity);
                return {};
            }

            Status redo(TxnExecCtx &context) override {
                return execute(context);
            }

          private:
            UUID m_scene;
            UUID m_entity;
            std::optional<SceneMutation> m_mutation;
            std::unique_ptr<SimulationRestorePoint> m_restorePoint;
        };

        class AddConnectionOperation final : public TransactionOperation {
          public:
            AddConnectionOperation(UUID scene,
                                   UUID connection,
                                   UUID source,
                                   UUID destination)
                : m_scene(scene),
                  m_connection(connection),
                  m_source(source),
                  m_destination(destination) {
            }

            std::string name() const override {
                return "Add connection";
            }

            Status execute(TxnExecCtx &context) override {
                auto *scene = requireScene(context.document, m_scene);
                if (!scene) {
                    return fail(Error::notFound("Scene does not exist"));
                }

                auto source = resolveEnginePort(
                    *scene, m_source, SimEngine::PortDirection::output);
                if (!source) {
                    return fail(std::move(source.error()));
                }
                auto destination = resolveEnginePort(
                    *scene, m_destination, SimEngine::PortDirection::input);
                if (!destination) {
                    return fail(std::move(destination.error()));
                }
                if (auto status =
                        context.simulation.connect(*source, *destination);
                    !status) {
                    return status;
                }

                EntityRecord record;
                record.identity.id = m_connection;
                record.identity.name = "Connection";
                record.identity.kind = EntityKind::connection;
                record.identity.showInProjectExplorer = false;
                record.interaction.draggable = true;
                record.connection = ConnectionComponent{
                    .startPort = m_source,
                    .endPort = m_destination,
                };
                auto created = scene->createEntity(std::move(record));
                if (!created) {
                    auto rollback =
                        context.simulation.disconnect(*source, *destination);
                    if (!rollback) {
                        return fail(Error::rollbackFailure(
                            created.error().message +
                            "; simulation rollback also failed: " +
                            rollback.error().message));
                    }
                    return fail(std::move(created.error()));
                }

                m_sourceRef = *source;
                m_destinationRef = *destination;
                recordChange(
                    context, ChangeKind::entityAdded, m_scene, m_connection);
                return {};
            }

            Status undo(TxnExecCtx &context) override {
                auto *scene = requireScene(context.document, m_scene);
                if (!scene || !m_sourceRef || !m_destinationRef) {
                    return fail(Error::invalidState(
                        "Connection has no executable state"));
                }

                auto mutation = scene->destroyEntity(m_connection);
                if (!mutation) {
                    return fail(std::move(mutation.error()));
                }
                if (auto status = context.simulation.disconnect(
                        *m_sourceRef, *m_destinationRef);
                    !status) {
                    auto rollback = scene->restoreMutation(*mutation);
                    if (!rollback) {
                        return fail(Error::rollbackFailure(
                            status.error().message +
                            "; scene rollback also failed: " +
                            rollback.error().message));
                    }
                    return status;
                }
                m_mutation = std::move(*mutation);
                recordChange(
                    context, ChangeKind::entityRemoved, m_scene, m_connection);
                return {};
            }

            Status redo(TxnExecCtx &context) override {
                auto *scene = requireScene(context.document, m_scene);
                if (!scene || !m_mutation) {
                    return fail(
                        Error::invalidState("Connection has no redo state"));
                }
                auto source = resolveEnginePort(
                    *scene, m_source, SimEngine::PortDirection::output);
                if (!source) {
                    return fail(std::move(source.error()));
                }
                auto destination = resolveEnginePort(
                    *scene, m_destination, SimEngine::PortDirection::input);
                if (!destination) {
                    return fail(std::move(destination.error()));
                }
                if (auto status =
                        context.simulation.connect(*source, *destination);
                    !status) {
                    return status;
                }
                if (auto status = scene->restoreMutation(*m_mutation);
                    !status) {
                    auto rollback =
                        context.simulation.disconnect(*source, *destination);
                    if (!rollback) {
                        return fail(Error::rollbackFailure(
                            status.error().message +
                            "; simulation rollback also failed: " +
                            rollback.error().message));
                    }
                    return status;
                }
                m_sourceRef = *source;
                m_destinationRef = *destination;
                recordChange(
                    context, ChangeKind::entityAdded, m_scene, m_connection);
                return {};
            }

          private:
            UUID m_scene;
            UUID m_connection;
            UUID m_source;
            UUID m_destination;
            std::optional<SimEngine::PortRef> m_sourceRef;
            std::optional<SimEngine::PortRef> m_destinationRef;
            std::optional<SceneMutation> m_mutation;
        };

        class RemoveConnectionOperation final : public TransactionOperation {
          public:
            RemoveConnectionOperation(UUID scene, UUID connection)
                : m_scene(scene),
                  m_connection(connection) {
            }

            std::string name() const override {
                return "Remove connection";
            }

            Status execute(TxnExecCtx &context) override {
                auto *scene = requireScene(context.document, m_scene);
                if (!scene) {
                    return fail(Error::notFound("Scene does not exist"));
                }
                const auto *connection =
                    scene->tryGet<ConnectionComponent>(m_connection);
                if (!connection) {
                    return fail(
                        Error::invalidArgument("Entity is not a connection"));
                }

                auto source =
                    resolveEnginePort(*scene,
                                      connection->startPort,
                                      SimEngine::PortDirection::output);
                if (!source) {
                    return fail(std::move(source.error()));
                }
                auto destination =
                    resolveEnginePort(*scene,
                                      connection->endPort,
                                      SimEngine::PortDirection::input);
                if (!destination) {
                    return fail(std::move(destination.error()));
                }
                if (auto status =
                        context.simulation.disconnect(*source, *destination);
                    !status) {
                    return status;
                }

                auto mutation = scene->destroyEntity(m_connection);
                if (!mutation) {
                    auto rollback =
                        context.simulation.connect(*source, *destination);
                    if (!rollback) {
                        return fail(Error::rollbackFailure(
                            mutation.error().message +
                            "; simulation rollback also failed: " +
                            rollback.error().message));
                    }
                    return fail(std::move(mutation.error()));
                }

                m_sourceRef = *source;
                m_destinationRef = *destination;
                m_mutation = std::move(*mutation);
                recordChange(
                    context, ChangeKind::entityRemoved, m_scene, m_connection);
                return {};
            }

            Status undo(TxnExecCtx &context) override {
                auto *scene = requireScene(context.document, m_scene);
                if (!scene || !m_sourceRef || !m_destinationRef ||
                    !m_mutation) {
                    return fail(Error::invalidState(
                        "Removed connection has no restorable state"));
                }
                if (auto status = context.simulation.connect(*m_sourceRef,
                                                             *m_destinationRef);
                    !status) {
                    return status;
                }
                if (auto status = scene->restoreMutation(*m_mutation);
                    !status) {
                    auto rollback = context.simulation.disconnect(
                        *m_sourceRef, *m_destinationRef);
                    if (!rollback) {
                        return fail(Error::rollbackFailure(
                            status.error().message +
                            "; simulation rollback also failed: " +
                            rollback.error().message));
                    }
                    return status;
                }
                recordChange(
                    context, ChangeKind::entityAdded, m_scene, m_connection);
                return {};
            }

            Status redo(TxnExecCtx &context) override {
                return execute(context);
            }

          private:
            UUID m_scene;
            UUID m_connection;
            std::optional<SimEngine::PortRef> m_sourceRef;
            std::optional<SimEngine::PortRef> m_destinationRef;
            std::optional<SceneMutation> m_mutation;
        };

        class AddPortOperation final : public TransactionOperation {
          public:
            AddPortOperation(UUID scene,
                             UUID node,
                             UUID port,
                             SimEngine::PortDirection direction,
                             SimEngine::SignalKind signalKind,
                             int index,
                             std::string portName)
                : m_scene(scene),
                  m_node(node),
                  m_port(port),
                  m_direction(direction),
                  m_signalKind(signalKind),
                  m_index(index),
                  m_name(std::move(portName)) {
            }

            std::string name() const override {
                return "Add simulation port";
            }

            Status execute(TxnExecCtx &context) override {
                auto *scene = requireScene(context.document, m_scene);
                if (!scene) {
                    return fail(Error::notFound("Scene does not exist"));
                }
                const auto *simulation =
                    scene->tryGet<SimulationComponent>(m_node);
                if (!simulation) {
                    return fail(Error::invalidArgument(
                        "Port owner is not a simulation component"));
                }
                auto before =
                    context.simulation.componentInfo(simulation->simulationId);
                if (!before) {
                    return fail(std::move(before.error()));
                }
                const auto &beforeDescriptor =
                    m_direction == SimEngine::PortDirection::input
                        ? before->inputs
                        : before->outputs;
                const auto &oppositeBefore =
                    m_direction == SimEngine::PortDirection::input
                        ? before->outputs
                        : before->inputs;
                const auto requestedIndex =
                    m_index < 0 ? static_cast<int>(beforeDescriptor.count)
                                : m_index;
                if (requestedIndex < 0 ||
                    static_cast<std::size_t>(requestedIndex) >
                        beforeDescriptor.count) {
                    return fail(Error::invalidArgument(
                        "New port index is outside the valid insertion range"));
                }

                m_enginePort = {
                    .componentId = simulation->simulationId,
                    .direction = m_direction,
                    .signalKind = m_signalKind,
                    .index = requestedIndex,
                };
                auto after = context.simulation.addPort(m_enginePort);
                if (!after) {
                    return fail(std::move(after.error()));
                }
                const auto &afterDescriptor =
                    m_direction == SimEngine::PortDirection::input
                        ? after->inputs
                        : after->outputs;
                const auto &oppositeAfter =
                    m_direction == SimEngine::PortDirection::input
                        ? after->outputs
                        : after->inputs;
                if (afterDescriptor.count != beforeDescriptor.count + 1 ||
                    oppositeAfter.count != oppositeBefore.count) {
                    auto rollback = context.simulation.removePort(m_enginePort);
                    if (!rollback) {
                        return fail(Error::rollbackFailure(
                            "The driver changed multiple port groups and the "
                            "rollback failed: " +
                            rollback.error().message));
                    }
                    return fail(Error::conflict("This driver couples port "
                                                "groups; use a driver-specific "
                                                "topology transaction"));
                }

                m_siblingsBefore.clear();
                const auto hierarchy =
                    scene->tryGet<HierarchyComponent>(m_node);
                if (!hierarchy) {
                    auto rollback = context.simulation.removePort(m_enginePort);
                    (void)rollback;
                    return fail(Error::invalidState(
                        "Simulation node hierarchy is missing"));
                }
                for (const auto child : hierarchy->children) {
                    auto *port = scene->tryGet<PortComponent>(child);
                    if (!port || port->resizeTrigger ||
                        port->direction != m_direction) {
                        continue;
                    }
                    m_siblingsBefore.emplace_back(child, *port);
                    if (port->index >= requestedIndex) {
                        ++port->index;
                    }
                }

                SimEngine::PortDescriptor descriptor = afterDescriptor;
                descriptor.direction = m_direction;
                descriptor.signalKind = m_signalKind;
                auto record = makePortRecord(
                    m_port,
                    m_node,
                    descriptor,
                    requestedIndex,
                    m_name.empty()
                        ? portName(descriptor,
                                   static_cast<std::size_t>(requestedIndex))
                        : m_name);
                auto created = scene->createEntity(std::move(record));
                if (!created) {
                    restoreSiblings(*scene);
                    auto rollback = context.simulation.removePort(m_enginePort);
                    if (!rollback) {
                        return fail(Error::rollbackFailure(
                            created.error().message +
                            "; simulation rollback also failed: " +
                            rollback.error().message));
                    }
                    return fail(std::move(created.error()));
                }

                recordChange(context, ChangeKind::entityAdded, m_scene, m_port);
                return {};
            }

            Status undo(TxnExecCtx &context) override {
                auto *scene = requireScene(context.document, m_scene);
                if (!scene) {
                    return fail(Error::notFound("Scene does not exist"));
                }
                auto mutation = scene->destroyEntity(m_port);
                if (!mutation) {
                    return fail(std::move(mutation.error()));
                }
                auto after = context.simulation.removePort(m_enginePort);
                if (!after) {
                    auto rollback = scene->restoreMutation(*mutation);
                    if (!rollback) {
                        return fail(Error::rollbackFailure(
                            after.error().message +
                            "; scene rollback also failed: " +
                            rollback.error().message));
                    }
                    return fail(std::move(after.error()));
                }
                restoreSiblings(*scene);
                m_mutation = std::move(*mutation);
                recordChange(
                    context, ChangeKind::entityRemoved, m_scene, m_port);
                return {};
            }

            Status redo(TxnExecCtx &context) override {
                auto *scene = requireScene(context.document, m_scene);
                if (!scene || !m_mutation) {
                    return fail(
                        Error::invalidState("Added port has no redo state"));
                }
                auto after = context.simulation.addPort(m_enginePort);
                if (!after) {
                    return fail(std::move(after.error()));
                }
                for (const auto &[id, previous] : m_siblingsBefore) {
                    if (auto *port = scene->tryGet<PortComponent>(id);
                        port && port->index >= m_enginePort.index) {
                        ++port->index;
                    }
                }
                if (auto status = scene->restoreMutation(*m_mutation);
                    !status) {
                    restoreSiblings(*scene);
                    auto rollback = context.simulation.removePort(m_enginePort);
                    if (!rollback) {
                        return fail(Error::rollbackFailure(
                            status.error().message +
                            "; simulation rollback also failed: " +
                            rollback.error().message));
                    }
                    return status;
                }
                recordChange(context, ChangeKind::entityAdded, m_scene, m_port);
                return {};
            }

          private:
            void restoreSiblings(SceneDocument &scene) const {
                for (const auto &[id, previous] : m_siblingsBefore) {
                    if (auto *port = scene.tryGet<PortComponent>(id)) {
                        *port = previous;
                    }
                }
            }

          private:
            UUID m_scene;
            UUID m_node;
            UUID m_port;
            SimEngine::PortDirection m_direction;
            SimEngine::SignalKind m_signalKind;
            int m_index;
            std::string m_name;
            SimEngine::PortRef m_enginePort;
            std::vector<std::pair<UUID, PortComponent>> m_siblingsBefore;
            std::optional<SceneMutation> m_mutation;
        };

        class RemovePortOperation final : public TransactionOperation {
          public:
            RemovePortOperation(UUID scene, UUID port)
                : m_scene(scene),
                  m_port(port) {
            }

            std::string name() const override {
                return "Remove simulation port";
            }

            Status execute(TxnExecCtx &context) override {
                auto *scene = requireScene(context.document, m_scene);
                if (!scene) {
                    return fail(Error::notFound("Scene does not exist"));
                }
                const auto *port = scene->tryGet<PortComponent>(m_port);
                const auto *hierarchy =
                    scene->tryGet<HierarchyComponent>(m_port);
                if (!port || !hierarchy || port->resizeTrigger ||
                    hierarchy->parent == UUID::null) {
                    return fail(
                        Error::invalidArgument("Entity is not a real port"));
                }
                if (!scene->connectionsForPort(m_port).empty()) {
                    return fail(Error::conflict(
                        "Disconnect a port before removing it"));
                }
                const auto *simulation =
                    scene->tryGet<SimulationComponent>(hierarchy->parent);
                if (!simulation) {
                    return fail(Error::invalidState(
                        "Port owner is not a simulation component"));
                }

                auto before =
                    context.simulation.componentInfo(simulation->simulationId);
                if (!before) {
                    return fail(std::move(before.error()));
                }
                const auto &beforeDescriptor =
                    port->direction == SimEngine::PortDirection::input
                        ? before->inputs
                        : before->outputs;
                const auto &oppositeBefore =
                    port->direction == SimEngine::PortDirection::input
                        ? before->outputs
                        : before->inputs;

                m_node = hierarchy->parent;
                m_enginePort = {
                    .componentId = simulation->simulationId,
                    .direction = port->direction,
                    .signalKind = port->signalKind,
                    .index = port->index,
                };
                auto after = context.simulation.removePort(m_enginePort);
                if (!after) {
                    return fail(std::move(after.error()));
                }
                const auto &afterDescriptor =
                    port->direction == SimEngine::PortDirection::input
                        ? after->inputs
                        : after->outputs;
                const auto &oppositeAfter =
                    port->direction == SimEngine::PortDirection::input
                        ? after->outputs
                        : after->inputs;
                if (afterDescriptor.count + 1 != beforeDescriptor.count ||
                    oppositeAfter.count != oppositeBefore.count) {
                    auto rollback = context.simulation.addPort(m_enginePort);
                    if (!rollback) {
                        return fail(Error::rollbackFailure(
                            "The driver changed multiple port groups and the "
                            "rollback failed: " +
                            rollback.error().message));
                    }
                    return fail(Error::conflict("This driver couples port "
                                                "groups; use a driver-specific "
                                                "topology transaction"));
                }

                m_siblingsBefore.clear();
                const auto *nodeHierarchy =
                    scene->tryGet<HierarchyComponent>(m_node);
                for (const auto child : nodeHierarchy->children) {
                    auto *sibling = scene->tryGet<PortComponent>(child);
                    if (!sibling || sibling->resizeTrigger ||
                        sibling->direction != port->direction) {
                        continue;
                    }
                    m_siblingsBefore.emplace_back(child, *sibling);
                    if (sibling->index > port->index) {
                        --sibling->index;
                    }
                }

                auto mutation = scene->destroyEntity(m_port);
                if (!mutation) {
                    restoreSiblings(*scene);
                    auto rollback = context.simulation.addPort(m_enginePort);
                    if (!rollback) {
                        return fail(Error::rollbackFailure(
                            mutation.error().message +
                            "; simulation rollback also failed: " +
                            rollback.error().message));
                    }
                    return fail(std::move(mutation.error()));
                }
                m_mutation = std::move(*mutation);
                recordChange(
                    context, ChangeKind::entityRemoved, m_scene, m_port);
                return {};
            }

            Status undo(TxnExecCtx &context) override {
                auto *scene = requireScene(context.document, m_scene);
                if (!scene || !m_mutation) {
                    return fail(Error::invalidState(
                        "Removed port has no restorable state"));
                }
                auto result = context.simulation.addPort(m_enginePort);
                if (!result) {
                    return fail(std::move(result.error()));
                }
                restoreSiblings(*scene);
                if (auto status = scene->restoreMutation(*m_mutation);
                    !status) {
                    auto rollback = context.simulation.removePort(m_enginePort);
                    if (!rollback) {
                        return fail(Error::rollbackFailure(
                            status.error().message +
                            "; simulation rollback also failed: " +
                            rollback.error().message));
                    }
                    return status;
                }
                recordChange(context, ChangeKind::entityAdded, m_scene, m_port);
                return {};
            }

            Status redo(TxnExecCtx &context) override {
                return execute(context);
            }

          private:
            void restoreSiblings(SceneDocument &scene) const {
                for (const auto &[id, previous] : m_siblingsBefore) {
                    if (auto *port = scene.tryGet<PortComponent>(id)) {
                        *port = previous;
                    }
                }
            }

          private:
            UUID m_scene;
            UUID m_port;
            UUID m_node = UUID::null;
            SimEngine::PortRef m_enginePort;
            std::vector<std::pair<UUID, PortComponent>> m_siblingsBefore;
            std::optional<SceneMutation> m_mutation;
        };
    } // namespace

    Transaction::Transaction(std::string name)
        : m_name(name.empty() ? "Edit" : std::move(name)) {
    }

    void Transaction::add(std::unique_ptr<TransactionOperation> operation) {
        if (operation && m_state == TransactionState::staged) {
            m_operations.push_back(std::move(operation));
        }
    }

    Status Transaction::execute(TxnExecCtx &context) {
        if (m_state != TransactionState::staged) {
            return fail(Error::invalidState(
                "Only a staged transaction can be executed"));
        }
        if (m_operations.empty()) {
            return fail(
                Error::invalidArgument("Cannot execute an empty transaction"));
        }

        std::size_t applied = 0;
        for (; applied < m_operations.size(); ++applied) {
            auto status = m_operations[applied]->execute(context);
            if (status) {
                continue;
            }

            const auto operationError = std::move(status.error());
            std::string rollbackError;
            while (applied > 0) {
                --applied;
                auto rollback = m_operations[applied]->undo(context);
                if (!rollback && rollbackError.empty()) {
                    rollbackError = rollback.error().message;
                }
            }
            if (!rollbackError.empty()) {
                return fail(Error::rollbackFailure(
                    operationError.message +
                    "; rollback failed: " + rollbackError));
            }
            return fail(operationError);
        }

        m_state = TransactionState::applied;
        return {};
    }

    Status Transaction::undo(TxnExecCtx &context) {
        if (m_state != TransactionState::applied) {
            return fail(Error::invalidState(
                "Only an applied transaction can be undone"));
        }

        for (std::size_t i = m_operations.size(); i > 0; --i) {
            auto status = m_operations[i - 1]->undo(context);
            if (status) {
                continue;
            }

            const auto operationError = std::move(status.error());
            std::string compensationError;
            for (std::size_t restore = i; restore < m_operations.size();
                 ++restore) {
                auto compensation = m_operations[restore]->redo(context);
                if (!compensation && compensationError.empty()) {
                    compensationError = compensation.error().message;
                }
            }
            if (!compensationError.empty()) {
                return fail(Error::rollbackFailure(
                    operationError.message +
                    "; undo compensation failed: " + compensationError));
            }
            return fail(operationError);
        }

        m_state = TransactionState::undone;
        return {};
    }

    Status Transaction::redo(TxnExecCtx &context) {
        if (m_state != TransactionState::undone) {
            return fail(Error::invalidState(
                "Only an undone transaction can be redone"));
        }

        std::size_t applied = 0;
        for (; applied < m_operations.size(); ++applied) {
            auto status = m_operations[applied]->redo(context);
            if (status) {
                continue;
            }
            const auto operationError = std::move(status.error());
            std::string rollbackError;
            while (applied > 0) {
                --applied;
                auto rollback = m_operations[applied]->undo(context);
                if (!rollback && rollbackError.empty()) {
                    rollbackError = rollback.error().message;
                }
            }
            if (!rollbackError.empty()) {
                return fail(Error::rollbackFailure(
                    operationError.message +
                    "; redo rollback failed: " + rollbackError));
            }
            return fail(operationError);
        }

        m_state = TransactionState::applied;
        return {};
    }

    const std::string &Transaction::name() const noexcept {
        return m_name;
    }

    std::size_t Transaction::size() const noexcept {
        return m_operations.size();
    }

    bool Transaction::empty() const noexcept {
        return m_operations.empty();
    }

    TransactionState Transaction::state() const noexcept {
        return m_state;
    }

    TransactionBuilder::TransactionBuilder(Transaction &transaction,
                                           const ProjectDocument &document)
        : m_transaction(&transaction),
          m_document(&document) {
    }

    UUID TransactionBuilder::activeScene() const noexcept {
        return m_document ? m_document->activeSceneId() : UUID::null;
    }

    const SceneDocument *TransactionBuilder::scene(UUID id) const noexcept {
        return m_document ? m_document->scene(id) : nullptr;
    }

    UUID TransactionBuilder::addScene(std::string name) {
        if (name.empty()) {
            name = "Scene";
        }
        const UUID id;
        addOperation(std::make_unique<AddSceneOperation>(id, std::move(name)));
        return id;
    }

    bool TransactionBuilder::removeScene(UUID sceneId) {
        const auto *target = scene(sceneId);
        if (!target) {
            abort(Error::notFound("Scene does not exist"));
            return false;
        }
        if (sceneId == m_document->rootSceneId()) {
            abort(Error::conflict("The root scene cannot be removed"));
            return false;
        }
        addOperation(std::make_unique<RemoveSceneOperation>(sceneId));
        return true;
    }

    bool TransactionBuilder::setActiveScene(UUID sceneId) {
        if (!scene(sceneId)) {
            abort(Error::notFound("Scene does not exist"));
            return false;
        }
        addOperation(std::make_unique<SetActiveSceneOperation>(sceneId));
        return true;
    }

    UUID TransactionBuilder::addEntity(EntityRecord record) {
        return addEntity(activeScene(), std::move(record));
    }

    UUID TransactionBuilder::addEntity(UUID sceneId, EntityRecord record) {
        if (!scene(sceneId)) {
            abort(Error::notFound("Scene does not exist"));
            return UUID::null;
        }
        if (record.identity.id == UUID::null) {
            record.identity.id = UUID{};
        }
        const auto id = record.identity.id;
        addOperation(
            std::make_unique<AddEntityOperation>(sceneId, std::move(record)));
        return id;
    }

    bool TransactionBuilder::removeEntity(UUID entity) {
        return removeEntity(activeScene(), entity);
    }

    bool TransactionBuilder::removeEntity(UUID sceneId, UUID entity) {
        const auto *targetScene = scene(sceneId);
        if (!targetScene || !targetScene->contains(entity)) {
            abort(Error::notFound("Scene entity does not exist"));
            return false;
        }
        if (targetScene->tryGet<SimulationComponent>(entity)) {
            return removeComp(sceneId, entity);
        }
        addOperation(std::make_unique<RemoveEntityOperation>(sceneId, entity));
        return true;
    }

    UUID TransactionBuilder::addComp(const SimEngine::CompDefRef &definition) {
        return addComp(activeScene(), definition);
    }

    UUID TransactionBuilder::addComp(UUID sceneId,
                                     const SimEngine::CompDefRef &definition,
                                     TransformComponent transform) {
        if (!scene(sceneId)) {
            abort(Error::notFound("Scene does not exist"));
            return UUID::null;
        }
        if (definition.name.empty()) {
            abort(Error::invalidArgument(
                "Simulation component definition name is empty"));
            return UUID::null;
        }
        const UUID id;
        addOperation(std::make_unique<AddSimulationOperation>(
            sceneId, id, definition, transform));
        return id;
    }

    bool TransactionBuilder::removeComp(UUID component) {
        return removeComp(activeScene(), component);
    }

    bool TransactionBuilder::removeComp(UUID sceneId, UUID component) {
        const auto *targetScene = scene(sceneId);
        if (!targetScene ||
            !targetScene->tryGet<SimulationComponent>(component)) {
            abort(
                Error::invalidArgument("Entity is not a simulation component"));
            return false;
        }

        std::vector<UUID> connections;
        auto dependants = targetScene->captureSubtree(component, true);
        if (!dependants) {
            abort(std::move(dependants.error()));
            return false;
        }
        for (const auto &record : *dependants) {
            if (record.connection) {
                connections.push_back(record.identity.id);
            }
        }
        std::sort(connections.begin(), connections.end(), [](UUID a, UUID b) {
            return static_cast<uint64_t>(a) < static_cast<uint64_t>(b);
        });
        connections.erase(std::unique(connections.begin(), connections.end()),
                          connections.end());
        for (const auto connection : connections) {
            addOperation(std::make_unique<RemoveConnectionOperation>(
                sceneId, connection));
        }

        addOperation(
            std::make_unique<RemoveSimulationOperation>(sceneId, component));

        if (const auto *module =
                targetScene->tryGet<ModuleComponent>(component);
            module && module->childScene != UUID::null) {
            if (module->childScene == m_document->rootSceneId()) {
                abort(Error::conflict("A module cannot own the root scene"));
                return false;
            }
            if (!scene(module->childScene)) {
                abort(Error::notFound("Module child scene does not exist"));
                return false;
            }
            addOperation(
                std::make_unique<RemoveSceneOperation>(module->childScene));
        }
        return true;
    }

    UUID TransactionBuilder::addConnection(UUID sourcePort,
                                           UUID destinationPort) {
        return addConnection(activeScene(), sourcePort, destinationPort);
    }

    UUID TransactionBuilder::addConnection(UUID sceneId,
                                           UUID sourcePort,
                                           UUID destinationPort) {
        const auto *targetScene = scene(sceneId);
        if (!targetScene || !targetScene->contains(sourcePort) ||
            !targetScene->contains(destinationPort)) {
            abort(Error::notFound(
                "One or both connection endpoints do not exist"));
            return UUID::null;
        }
        const UUID id;
        addOperation(std::make_unique<AddConnectionOperation>(
            sceneId, id, sourcePort, destinationPort));
        return id;
    }

    UUID TransactionBuilder::sceneEntityForSimulationId(
        UUID simulationId) const noexcept {
        if (!m_document) {
            return UUID::null;
        }
        for (const auto sceneId : m_document->sceneOrder()) {
            const auto *targetScene = m_document->scene(sceneId);
            const auto view =
                targetScene->registry()
                    .view<IdentityComponent, SimulationComponent>();
            for (const auto handle : view) {
                const auto &[identity, simulation] =
                    view.get<IdentityComponent, SimulationComponent>(handle);
                if (simulation.simulationId == simulationId) {
                    return identity.id;
                }
            }
        }
        return UUID::null;
    }

    UUID TransactionBuilder::scenePortFor(
        const SimEngine::PortRef &port) const noexcept {
        if (!m_document || !port.isValid()) {
            return UUID::null;
        }
        for (const auto sceneId : m_document->sceneOrder()) {
            const auto *targetScene = m_document->scene(sceneId);
            const auto view = targetScene->registry()
                                  .view<IdentityComponent,
                                        HierarchyComponent,
                                        PortComponent>();
            for (const auto handle : view) {
                const auto &[identity, hierarchy, candidate] =
                    view.get<IdentityComponent,
                             HierarchyComponent,
                             PortComponent>(handle);
                if (candidate.direction != port.direction ||
                    candidate.signalKind != port.signalKind ||
                    candidate.index != port.index) {
                    continue;
                }
                const auto *owner =
                    targetScene->tryGet<SimulationComponent>(hierarchy.parent);
                if (owner && owner->simulationId == port.componentId) {
                    return identity.id;
                }
            }
        }
        return UUID::null;
    }

    UUID
    TransactionBuilder::addConnection(const SimEngine::PortRef &source,
                                      const SimEngine::PortRef &destination) {
        const auto sourceEntity = scenePortFor(source);
        const auto destinationEntity = scenePortFor(destination);
        if (sourceEntity == UUID::null || destinationEntity == UUID::null) {
            abort(Error::notFound(
                "Simulation port does not have a scene entity"));
            return UUID::null;
        }
        for (const auto sceneId : m_document->sceneOrder()) {
            const auto *candidate = scene(sceneId);
            if (candidate->contains(sourceEntity) &&
                candidate->contains(destinationEntity)) {
                return addConnection(sceneId, sourceEntity, destinationEntity);
            }
        }
        abort(Error::conflict("Connections cannot span multiple scenes"));
        return UUID::null;
    }

    bool TransactionBuilder::removeConnection(UUID connection) {
        return removeConnection(activeScene(), connection);
    }

    bool TransactionBuilder::removeConnection(UUID sceneId, UUID connection) {
        const auto *targetScene = scene(sceneId);
        if (!targetScene ||
            !targetScene->tryGet<ConnectionComponent>(connection)) {
            abort(Error::invalidArgument("Entity is not a connection"));
            return false;
        }
        addOperation(
            std::make_unique<RemoveConnectionOperation>(sceneId, connection));
        return true;
    }

    UUID TransactionBuilder::addPort(const SimEngine::PortRef &port) {
        const auto node = sceneEntityForSimulationId(port.componentId);
        if (node == UUID::null) {
            abort(Error::notFound(
                "Simulation component does not have a scene entity"));
            return UUID::null;
        }
        for (const auto sceneId : m_document->sceneOrder()) {
            if (scene(sceneId)->contains(node)) {
                return addPort(
                    sceneId, node, port.direction, port.signalKind, port.index);
            }
        }
        abort(Error::notFound("Owning scene does not exist"));
        return UUID::null;
    }

    UUID TransactionBuilder::addPort(UUID sceneId,
                                     UUID simulationEntity,
                                     SimEngine::PortDirection direction,
                                     SimEngine::SignalKind signalKind,
                                     int index,
                                     std::string name) {
        const auto *targetScene = scene(sceneId);
        if (!targetScene ||
            !targetScene->tryGet<SimulationComponent>(simulationEntity)) {
            abort(Error::invalidArgument(
                "Port owner is not a simulation component"));
            return UUID::null;
        }
        if (direction == SimEngine::PortDirection::none ||
            signalKind == SimEngine::SignalKind::none) {
            abort(Error::invalidArgument(
                "Port direction and signal kind are required"));
            return UUID::null;
        }
        const UUID id;
        addOperation(std::make_unique<AddPortOperation>(sceneId,
                                                        simulationEntity,
                                                        id,
                                                        direction,
                                                        signalKind,
                                                        index,
                                                        std::move(name)));
        return id;
    }

    bool TransactionBuilder::removePort(UUID port) {
        return removePort(activeScene(), port);
    }

    bool TransactionBuilder::removePort(UUID sceneId, UUID port) {
        const auto *targetScene = scene(sceneId);
        if (!targetScene || !targetScene->tryGet<PortComponent>(port)) {
            abort(Error::invalidArgument("Entity is not a port"));
            return false;
        }
        const auto connections = targetScene->connectionsForPort(port);
        for (const auto connection : connections) {
            addOperation(std::make_unique<RemoveConnectionOperation>(
                sceneId, connection));
        }
        addOperation(std::make_unique<RemovePortOperation>(sceneId, port));
        return true;
    }

    void TransactionBuilder::addOperation(
        std::unique_ptr<TransactionOperation> operation) {
        if (!m_error && operation) {
            m_transaction->add(std::move(operation));
        }
    }

    void TransactionBuilder::abort(Error error) {
        if (!m_error) {
            m_error = std::move(error);
        }
    }

    bool TransactionBuilder::valid() const noexcept {
        return !m_error.has_value();
    }

    const std::optional<Error> &TransactionBuilder::error() const noexcept {
        return m_error;
    }
} // namespace Bess::Session
