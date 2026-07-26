#pragma once

#include "common/bess_api.h"
#include "project_session/project_document.h"
#include "project_session/project_store.h"
#include "project_session/result.h"
#include "project_session/session_event.h"
#include "project_session/simulation_gateway.h"
#include "project_session/transaction.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Bess::Session {
    class ProjectSession;

    class BESS_API ProjectReadAccess {
      public:
        ProjectReadAccess(ProjectReadAccess &&) noexcept = default;
        ProjectReadAccess &operator=(ProjectReadAccess &&) noexcept = default;

        ProjectReadAccess(const ProjectReadAccess &) = delete;
        ProjectReadAccess &operator=(const ProjectReadAccess &) = delete;

        const ProjectDocument &get() const noexcept;
        const ProjectDocument *operator->() const noexcept;
        const ProjectDocument &operator*() const noexcept;

      private:
        friend class ProjectSession;

        ProjectReadAccess(std::shared_mutex &mutex,
                          const ProjectDocument &document);

        std::shared_lock<std::shared_mutex> m_lock;
        const ProjectDocument *m_document;
    };

    class BESS_API ProjectSession {
      public:
        using Observer = std::function<void(const SessionChangeSet &)>;
        using ObserverToken = uint64_t;

        explicit ProjectSession(
            std::shared_ptr<ISimulationGateway> simulation = nullptr,
            std::size_t historyLimit = 256);
        ~ProjectSession() = default;

        ProjectSession(const ProjectSession &) = delete;
        ProjectSession &operator=(const ProjectSession &) = delete;
        ProjectSession(ProjectSession &&) = delete;
        ProjectSession &operator=(ProjectSession &&) = delete;

        ProjectReadAccess read() const;

        Status transact(const TTransactionFn &function,
                        std::string name = "Edit");
        Status execute(Transaction transaction);
        Status undo();
        Status redo();
        bool canUndo() const;
        bool canRedo() const;
        std::string nextUndoName() const;
        std::string nextRedoName() const;
        void clearHistory();

        Status createNewProject(std::string name = "Untitled Project");
        Status loadProject(IProjectStore &store,
                           const std::filesystem::path &path);
        Status saveProject(IProjectStore &store);
        Status saveProjectAs(IProjectStore &store,
                             const std::filesystem::path &path);

        bool isDirty() const;
        bool hasPath() const;
        std::optional<std::filesystem::path> projectPath() const;
        uint64_t revision() const;

        Result<SimEngine::PortState> portState(UUID scene, UUID port) const;
        SimEngine::SimulationState simulationState() const;
        Status setSimulationState(SimEngine::SimulationState state);
        Status stepSimulation();

        ObserverToken subscribe(Observer observer);
        bool unsubscribe(ObserverToken token);

      private:
        struct HistoryEntry {
            std::unique_ptr<Transaction> transaction;
            uint64_t beforeRevision = 0;
            uint64_t afterRevision = 0;
        };

        Status executeLocked(std::unique_ptr<Transaction> transaction,
                             SessionChangeSet &changes);
        Status hydrateSimulation(ProjectDocument &document);
        Result<SimEngine::PortRef>
        resolvePort(const ProjectDocument &document,
                    UUID scene,
                    UUID port,
                    std::optional<SimEngine::PortDirection> requiredDirection =
                        std::nullopt) const;
        void trimHistory();
        void notify(const SessionChangeSet &changes) const;

      private:
        mutable std::shared_mutex m_mutex;
        std::unique_ptr<ProjectDocument> m_document;
        std::shared_ptr<ISimulationGateway> m_simulation;
        std::vector<HistoryEntry> m_undoHistory;
        std::vector<HistoryEntry> m_redoHistory;
        std::size_t m_historyLimit;
        uint64_t m_currentRevision = 0;
        uint64_t m_savedRevision = 0;
        uint64_t m_nextRevision = 1;
        std::optional<std::filesystem::path> m_projectPath;

        mutable std::shared_mutex m_observerMutex;
        std::unordered_map<ObserverToken, Observer> m_observers;
        ObserverToken m_nextObserverToken = 1;
    };
} // namespace Bess::Session
