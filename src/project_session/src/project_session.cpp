#include "project_session/project_session.h"

#include <algorithm>
#include <mutex>
#include <utility>

namespace Bess::Session {
    namespace {
        class UnavailableSimulationGateway final : public ISimulationGateway {
          public:
            Result<SimulationComponentInfo>
            createComponent(const SimEngine::CompDefRef &) override {
                return unavailable();
            }

            Result<std::unique_ptr<SimulationRestorePoint>>
            removeComponent(UUID) override {
                return fail(Error::simulationFailure(
                    "No simulation gateway is attached to this session"));
            }

            Result<SimulationComponentInfo>
            restoreComponent(const SimulationRestorePoint &) override {
                return unavailable();
            }

            Result<SimulationComponentInfo> componentInfo(UUID) const override {
                return unavailable();
            }

            Status connect(const SimEngine::PortRef &,
                           const SimEngine::PortRef &) override {
                return unavailableStatus();
            }

            Status disconnect(const SimEngine::PortRef &,
                              const SimEngine::PortRef &) override {
                return unavailableStatus();
            }

            Result<SimulationComponentInfo>
            addPort(const SimEngine::PortRef &) override {
                return unavailable();
            }

            Result<SimulationComponentInfo>
            removePort(const SimEngine::PortRef &) override {
                return unavailable();
            }

            Result<SimEngine::PortState>
            portState(const SimEngine::PortRef &) const override {
                return fail(Error::simulationFailure(
                    "No simulation gateway is attached to this session"));
            }

            SimEngine::SimulationState simulationState() const override {
                return SimEngine::SimulationState::stopped;
            }

            Status setSimulationState(SimEngine::SimulationState) override {
                return unavailableStatus();
            }

            Status stepSimulation() override {
                return unavailableStatus();
            }

            Status reset() override {
                return {};
            }

          private:
            static Result<SimulationComponentInfo> unavailable() {
                return fail(Error::simulationFailure(
                    "No simulation gateway is attached to this session"));
            }

            static Status unavailableStatus() {
                return fail(Error::simulationFailure(
                    "No simulation gateway is attached to this session"));
            }
        };
    } // namespace

    ProjectReadAccess::ProjectReadAccess(std::shared_mutex &mutex,
                                         const ProjectDocument &document)
        : m_lock(mutex),
          m_document(&document) {
    }

    const ProjectDocument &ProjectReadAccess::get() const noexcept {
        return *m_document;
    }

    const ProjectDocument *ProjectReadAccess::operator->() const noexcept {
        return m_document;
    }

    const ProjectDocument &ProjectReadAccess::operator*() const noexcept {
        return *m_document;
    }

    ProjectSession::ProjectSession(
        std::shared_ptr<ISimulationGateway> simulation,
        std::size_t historyLimit)
        : m_document(ProjectDocument::createEmpty()),
          m_simulation(simulation
                           ? std::move(simulation)
                           : std::make_shared<UnavailableSimulationGateway>()),
          m_historyLimit(std::max<std::size_t>(1, historyLimit)) {
    }

    ProjectReadAccess ProjectSession::read() const {
        return ProjectReadAccess(m_mutex, *m_document);
    }

    Status ProjectSession::transact(const TTransactionFn &function,
                                    std::string name) {
        if (!function) {
            return fail(
                Error::invalidArgument("Transaction callback is empty"));
        }

        SessionChangeSet changes;
        Status status;
        {
            std::unique_lock lock(m_mutex);
            auto transaction = std::make_unique<Transaction>(std::move(name));
            TransactionBuilder builder(*transaction, *m_document);
            try {
                function(builder);
            } catch (const std::exception &exception) {
                return fail(Error::transactionFailure(
                    "Transaction staging threw an exception: " +
                    std::string(exception.what())));
            } catch (...) {
                return fail(Error::transactionFailure(
                    "Transaction staging threw an unknown exception"));
            }

            if (!builder.valid()) {
                return fail(*builder.error());
            }
            status = executeLocked(std::move(transaction), changes);
        }
        if (status) {
            notify(changes);
        }
        return status;
    }

    Status ProjectSession::execute(Transaction transaction) {
        SessionChangeSet changes;
        Status status;
        {
            std::unique_lock lock(m_mutex);
            status = executeLocked(
                std::make_unique<Transaction>(std::move(transaction)), changes);
        }
        if (status) {
            notify(changes);
        }
        return status;
    }

    Status
    ProjectSession::executeLocked(std::unique_ptr<Transaction> transaction,
                                  SessionChangeSet &changes) {
        if (!transaction || transaction->empty()) {
            return fail(
                Error::invalidArgument("Cannot execute an empty transaction"));
        }

        TxnExecCtx context{*m_document, *m_simulation, changes};
        if (auto status = transaction->execute(context); !status) {
            return status;
        }
        if (auto validation = m_document->validate(); !validation) {
            auto rollback = transaction->undo(context);
            if (!rollback) {
                return fail(Error::rollbackFailure(
                    "Document validation failed: " +
                    validation.error().message +
                    "; transaction rollback also failed: " +
                    rollback.error().message));
            }
            return fail(Error::transactionFailure(
                "Document validation failed: " + validation.error().message));
        }

        const auto before = m_currentRevision;
        const auto after = m_nextRevision++;
        m_currentRevision = after;
        m_redoHistory.clear();
        m_undoHistory.push_back({std::move(transaction), before, after});
        trimHistory();
        return {};
    }

    Status ProjectSession::undo() {
        SessionChangeSet changes;
        {
            std::unique_lock lock(m_mutex);
            if (m_undoHistory.empty()) {
                return fail(Error::invalidState("There is nothing to undo"));
            }

            auto &entry = m_undoHistory.back();
            TxnExecCtx context{*m_document, *m_simulation, changes};
            if (auto status = entry.transaction->undo(context); !status) {
                return status;
            }
            if (auto validation = m_document->validate(); !validation) {
                auto compensation = entry.transaction->redo(context);
                if (!compensation) {
                    return fail(
                        Error::rollbackFailure("Undo validation failed: " +
                                               validation.error().message +
                                               "; compensation also failed: " +
                                               compensation.error().message));
                }
                return fail(Error::transactionFailure(
                    "Undo validation failed: " + validation.error().message));
            }

            m_currentRevision = entry.beforeRevision;
            changes.push_back({ChangeKind::transactionUndone,
                               UUID::null,
                               UUID::null,
                               entry.transaction->name()});
            m_redoHistory.push_back(std::move(entry));
            m_undoHistory.pop_back();
        }
        notify(changes);
        return {};
    }

    Status ProjectSession::redo() {
        SessionChangeSet changes;
        {
            std::unique_lock lock(m_mutex);
            if (m_redoHistory.empty()) {
                return fail(Error::invalidState("There is nothing to redo"));
            }

            auto &entry = m_redoHistory.back();
            TxnExecCtx context{*m_document, *m_simulation, changes};
            if (auto status = entry.transaction->redo(context); !status) {
                return status;
            }
            if (auto validation = m_document->validate(); !validation) {
                auto rollback = entry.transaction->undo(context);
                if (!rollback) {
                    return fail(Error::rollbackFailure(
                        "Redo validation failed: " +
                        validation.error().message +
                        "; rollback also failed: " + rollback.error().message));
                }
                return fail(Error::transactionFailure(
                    "Redo validation failed: " + validation.error().message));
            }

            m_currentRevision = entry.afterRevision;
            changes.push_back({ChangeKind::transactionRedone,
                               UUID::null,
                               UUID::null,
                               entry.transaction->name()});
            m_undoHistory.push_back(std::move(entry));
            m_redoHistory.pop_back();
            trimHistory();
        }
        notify(changes);
        return {};
    }

    bool ProjectSession::canUndo() const {
        std::shared_lock lock(m_mutex);
        return !m_undoHistory.empty();
    }

    bool ProjectSession::canRedo() const {
        std::shared_lock lock(m_mutex);
        return !m_redoHistory.empty();
    }

    std::string ProjectSession::nextUndoName() const {
        std::shared_lock lock(m_mutex);
        return m_undoHistory.empty() ? std::string{}
                                     : m_undoHistory.back().transaction->name();
    }

    std::string ProjectSession::nextRedoName() const {
        std::shared_lock lock(m_mutex);
        return m_redoHistory.empty() ? std::string{}
                                     : m_redoHistory.back().transaction->name();
    }

    void ProjectSession::clearHistory() {
        std::unique_lock lock(m_mutex);
        m_undoHistory.clear();
        m_redoHistory.clear();
    }

    Status ProjectSession::createNewProject(std::string name) {
        SessionChangeSet changes;
        {
            std::unique_lock lock(m_mutex);
            if (auto status = m_simulation->reset(); !status) {
                return status;
            }
            m_document = ProjectDocument::createEmpty(std::move(name));
            m_undoHistory.clear();
            m_redoHistory.clear();
            m_currentRevision = m_nextRevision++;
            m_savedRevision = m_currentRevision;
            m_projectPath.reset();
            changes.push_back({ChangeKind::projectReset});
        }
        notify(changes);
        return {};
    }

    Status ProjectSession::hydrateSimulation(ProjectDocument &document) {
        for (const auto sceneId : document.sceneOrder()) {
            auto *scene = document.scene(sceneId);
            const auto simulationView =
                scene->registry()
                    .view<IdentityComponent, SimulationComponent>();
            std::vector<UUID> simulationEntities;
            simulationEntities.reserve(simulationView.size_hint());
            for (const auto handle : simulationView) {
                simulationEntities.push_back(
                    simulationView.get<IdentityComponent>(handle).id);
            }

            for (const auto entityId : simulationEntities) {
                auto *simulation = scene->tryGet<SimulationComponent>(entityId);
                auto created =
                    m_simulation->createComponent(simulation->definition);
                if (!created) {
                    return fail(std::move(created.error()));
                }
                simulation->simulationId = created->id;
            }
        }

        for (const auto sceneId : document.sceneOrder()) {
            auto *scene = document.scene(sceneId);
            const auto connectionView =
                scene->registry().view<ConnectionComponent>();
            for (const auto handle : connectionView) {
                const auto &connection =
                    connectionView.get<ConnectionComponent>(handle);
                auto source = resolvePort(document,
                                          sceneId,
                                          connection.startPort,
                                          SimEngine::PortDirection::output);
                if (!source) {
                    return fail(std::move(source.error()));
                }
                auto destination = resolvePort(document,
                                               sceneId,
                                               connection.endPort,
                                               SimEngine::PortDirection::input);
                if (!destination) {
                    return fail(std::move(destination.error()));
                }
                if (auto status = m_simulation->connect(*source, *destination);
                    !status) {
                    return status;
                }
            }
        }
        return {};
    }

    Status ProjectSession::loadProject(IProjectStore &store,
                                       const std::filesystem::path &path) {
        if (path.empty()) {
            return fail(Error::invalidArgument("Project path is empty"));
        }

        auto loaded = store.load(path);
        if (!loaded) {
            return fail(std::move(loaded.error()));
        }
        if (!*loaded) {
            return fail(Error{
                ErrorCode::persistenceFailure,
                "Project store returned a null document",
            });
        }
        if (auto status = (*loaded)->validate(); !status) {
            return fail(Error{
                ErrorCode::persistenceFailure,
                "Loaded project is invalid: " + status.error().message,
            });
        }

        SessionChangeSet changes;
        {
            std::unique_lock lock(m_mutex);
            if (auto status = m_simulation->reset(); !status) {
                return status;
            }
            if (auto status = hydrateSimulation(**loaded); !status) {
                auto reset = m_simulation->reset();
                if (!reset) {
                    return fail(Error::rollbackFailure(
                        status.error().message +
                        "; failed to reset partial simulation state: " +
                        reset.error().message));
                }
                auto restore = hydrateSimulation(*m_document);
                if (!restore) {
                    return fail(Error::rollbackFailure(
                        status.error().message +
                        "; previous simulation could not be restored: " +
                        restore.error().message));
                }
                return status;
            }

            m_document = std::move(*loaded);
            m_projectPath = path;
            m_undoHistory.clear();
            m_redoHistory.clear();
            m_currentRevision = m_nextRevision++;
            m_savedRevision = m_currentRevision;
            changes.push_back({ChangeKind::projectReset});
        }
        notify(changes);
        return {};
    }

    Status ProjectSession::saveProject(IProjectStore &store) {
        std::optional<std::filesystem::path> path;
        {
            std::shared_lock lock(m_mutex);
            path = m_projectPath;
        }
        if (!path) {
            return fail(Error{
                ErrorCode::persistenceFailure,
                "Project has no path; use saveProjectAs",
            });
        }
        return saveProjectAs(store, *path);
    }

    Status ProjectSession::saveProjectAs(IProjectStore &store,
                                         const std::filesystem::path &path) {
        if (path.empty()) {
            return fail(Error::invalidArgument("Project path is empty"));
        }

        SessionChangeSet changes;
        {
            std::unique_lock lock(m_mutex);
            if (auto status = store.save(path, *m_document); !status) {
                return status;
            }
            m_projectPath = path;
            m_savedRevision = m_currentRevision;
            changes.push_back({ChangeKind::saved});
        }
        notify(changes);
        return {};
    }

    bool ProjectSession::isDirty() const {
        std::shared_lock lock(m_mutex);
        return !m_projectPath.has_value() ||
               m_currentRevision != m_savedRevision;
    }

    bool ProjectSession::hasPath() const {
        std::shared_lock lock(m_mutex);
        return m_projectPath.has_value();
    }

    std::optional<std::filesystem::path> ProjectSession::projectPath() const {
        std::shared_lock lock(m_mutex);
        return m_projectPath;
    }

    uint64_t ProjectSession::revision() const {
        std::shared_lock lock(m_mutex);
        return m_currentRevision;
    }

    Result<SimEngine::PortRef> ProjectSession::resolvePort(
        const ProjectDocument &document,
        UUID sceneId,
        UUID portId,
        std::optional<SimEngine::PortDirection> requiredDirection) const {
        const auto *scene = document.scene(sceneId);
        if (!scene) {
            return fail(Error::notFound("Scene does not exist"));
        }

        const auto *proxy = scene->tryGet<ProxyPortComponent>(portId);
        if (proxy) {
            portId = requiredDirection == SimEngine::PortDirection::input
                         ? proxy->inputPort
                         : (proxy->outputPort != UUID::null ? proxy->outputPort
                                                            : proxy->inputPort);
        }
        const auto *port = scene->tryGet<PortComponent>(portId);
        const auto *hierarchy = scene->tryGet<HierarchyComponent>(portId);
        if (!port || !hierarchy || hierarchy->parent == UUID::null ||
            port->index < 0) {
            return fail(
                Error::invalidArgument("Entity is not a simulation port"));
        }
        if (requiredDirection && port->direction != *requiredDirection) {
            return fail(Error::conflict(
                "Port has the wrong direction for this operation"));
        }
        const auto *simulation =
            scene->tryGet<SimulationComponent>(hierarchy->parent);
        if (!simulation) {
            return fail(Error::invalidState("Port owner is not simulated"));
        }
        return SimEngine::PortRef{
            .componentId = simulation->simulationId,
            .direction = port->direction,
            .signalKind = port->signalKind,
            .index = port->index,
        };
    }

    Result<SimEngine::PortState> ProjectSession::portState(UUID scene,
                                                           UUID port) const {
        std::shared_lock lock(m_mutex);
        auto reference = resolvePort(*m_document, scene, port);
        if (!reference) {
            return fail(std::move(reference.error()));
        }
        return m_simulation->portState(*reference);
    }

    SimEngine::SimulationState ProjectSession::simulationState() const {
        std::shared_lock lock(m_mutex);
        return m_simulation->simulationState();
    }

    Status
    ProjectSession::setSimulationState(SimEngine::SimulationState state) {
        SessionChangeSet changes;
        {
            std::unique_lock lock(m_mutex);
            if (auto status = m_simulation->setSimulationState(state);
                !status) {
                return status;
            }
            changes.push_back({ChangeKind::simulationStateChanged});
        }
        notify(changes);
        return {};
    }

    Status ProjectSession::stepSimulation() {
        std::shared_lock lock(m_mutex);
        return m_simulation->stepSimulation();
    }

    ProjectSession::ObserverToken ProjectSession::subscribe(Observer observer) {
        if (!observer) {
            return 0;
        }
        std::unique_lock lock(m_observerMutex);
        const auto token = m_nextObserverToken++;
        m_observers.emplace(token, std::move(observer));
        return token;
    }

    bool ProjectSession::unsubscribe(ObserverToken token) {
        std::unique_lock lock(m_observerMutex);
        return m_observers.erase(token) != 0;
    }

    void ProjectSession::trimHistory() {
        if (m_undoHistory.size() <= m_historyLimit) {
            return;
        }
        const auto excess = m_undoHistory.size() - m_historyLimit;
        m_undoHistory.erase(m_undoHistory.begin(),
                            m_undoHistory.begin() +
                                static_cast<std::ptrdiff_t>(excess));
    }

    void ProjectSession::notify(const SessionChangeSet &changes) const {
        if (changes.empty()) {
            return;
        }
        std::vector<Observer> observers;
        {
            std::shared_lock lock(m_observerMutex);
            observers.reserve(m_observers.size());
            for (const auto &[token, observer] : m_observers) {
                (void)token;
                observers.push_back(observer);
            }
        }
        for (const auto &observer : observers) {
            try {
                observer(changes);
            } catch (...) {
                // Observers run after commit and cannot invalidate it.
            }
        }
    }
} // namespace Bess::Session
