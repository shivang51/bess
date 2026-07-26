#pragma once

#include "common/bess_api.h"
#include "common/bess_uuid.h"
#include "common/types.h"
#include "project_session/project_document.h"
#include "project_session/result.h"
#include "project_session/session_event.h"
#include "project_session/simulation_gateway.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Bess::Session {
    struct BESS_API TxnExecCtx {
        ProjectDocument &document;
        ISimulationGateway &simulation;
        SessionChangeSet &changes;
    };

    class BESS_API TransactionOperation {
      public:
        virtual ~TransactionOperation() = default;

        virtual std::string name() const = 0;
        virtual Status execute(TxnExecCtx &context) = 0;
        virtual Status undo(TxnExecCtx &context) = 0;
        virtual Status redo(TxnExecCtx &context) = 0;
    };

    enum class TransactionState : uint8_t {
        staged,
        applied,
        undone,
    };

    class BESS_API Transaction {
      public:
        explicit Transaction(std::string name = "Edit");
        ~Transaction() = default;

        Transaction(const Transaction &) = delete;
        Transaction &operator=(const Transaction &) = delete;
        Transaction(Transaction &&) noexcept = default;
        Transaction &operator=(Transaction &&) noexcept = default;

        void add(std::unique_ptr<TransactionOperation> operation);
        Status execute(TxnExecCtx &context);
        Status undo(TxnExecCtx &context);
        Status redo(TxnExecCtx &context);

        const std::string &name() const noexcept;
        std::size_t size() const noexcept;
        bool empty() const noexcept;
        TransactionState state() const noexcept;

      private:
        std::string m_name;
        std::vector<std::unique_ptr<TransactionOperation>> m_operations;
        TransactionState m_state = TransactionState::staged;
    };

    class BESS_API TransactionBuilder {
      public:
        TransactionBuilder(Transaction &transaction,
                           const ProjectDocument &document);

        UUID addScene(std::string name = "Scene");
        bool removeScene(UUID scene);
        bool setActiveScene(UUID scene);

        UUID addEntity(EntityRecord record);
        UUID addEntity(UUID scene, EntityRecord record);
        bool removeEntity(UUID entity);
        bool removeEntity(UUID scene, UUID entity);

        UUID addComp(const SimEngine::CompDefRef &definition);
        UUID addComp(UUID scene,
                     const SimEngine::CompDefRef &definition,
                     TransformComponent transform = {});
        bool removeComp(UUID component);
        bool removeComp(UUID scene, UUID component);

        UUID addConnection(UUID sourcePort, UUID destinationPort);
        UUID addConnection(UUID scene, UUID sourcePort, UUID destinationPort);
        UUID addConnection(const SimEngine::PortRef &source,
                           const SimEngine::PortRef &destination);
        bool removeConnection(UUID connection);
        bool removeConnection(UUID scene, UUID connection);

        UUID addPort(const SimEngine::PortRef &port);
        UUID addPort(UUID scene,
                     UUID simulationEntity,
                     SimEngine::PortDirection direction,
                     SimEngine::SignalKind signalKind,
                     int index = -1,
                     std::string name = {});
        bool removePort(UUID port);
        bool removePort(UUID scene, UUID port);

        void addOperation(std::unique_ptr<TransactionOperation> operation);
        void abort(Error error);
        bool valid() const noexcept;
        const std::optional<Error> &error() const noexcept;

      private:
        UUID activeScene() const noexcept;
        const SceneDocument *scene(UUID id) const noexcept;
        UUID sceneEntityForSimulationId(UUID simulationId) const noexcept;
        UUID scenePortFor(const SimEngine::PortRef &port) const noexcept;

      private:
        Transaction *m_transaction;
        const ProjectDocument *m_document;
        std::optional<Error> m_error;
    };

    using TransactionCtx = TransactionBuilder;
    using TTransactionFn = std::function<void(TransactionCtx &)>;
} // namespace Bess::Session
