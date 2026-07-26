#pragma once
#include "common/bess_api.h"
#include "common/bess_uuid.h"
#include "common/class_helpers.h"
#include "common/types.h"

namespace Bess::Session {

    // Transaction execution context, which is passed to the execute
    // function.
    // Idea is that this will contain the scene driver, simengine and other
    // relevant stuff
    struct BESS_API TxnExecCtx {};

    class BESS_API TransactionOp {
      public:
        virtual ~TransactionOp() = default;

        virtual bool execute(TxnExecCtx &ctx) = 0;
        virtual bool undo(TxnExecCtx &ctx) = 0;
        virtual bool redo(TxnExecCtx &ctx) = 0;

      protected:
        UUID m_id; // contr automatically creates an id
    };

    class BESS_API TransactionCtx {
      public:
        UUID addComp(const SimEngine::CompDefRef &compDef);
        bool removeComp(const Bess::UUID &compId);

        UUID addConnection(const Bess::SimEngine::PortRef &src,
                           const Bess::SimEngine::PortRef &dst);
        bool removeConnection(const Bess::UUID &connId);

        UUID addPort(const SimEngine::PortRef &portRef);
        bool removePort(const Bess::UUID &portId);

        bool execute(TxnExecCtx &ctx) {
            std::stack<std::unique_ptr<TransactionOp>> successOps;
            for (auto &op : m_ops) {
                const auto res = op->execute(ctx);
                if (res) {
                    successOps.push(std::move(op));
                } else {
                    undoStack(successOps, ctx);
                    return false;
                }
            }

            m_ops.clear();
            moveStackToVec(successOps);

            return true;
        }

        bool undo(TxnExecCtx &ctx);
        bool redo(TxnExecCtx &ctx);

      private:
        void undoStack(std::stack<std::unique_ptr<TransactionOp>> &opsStack,
                       TxnExecCtx &ctx) {
            while (!opsStack.empty()) {
                auto op = std::move(opsStack.top());
                opsStack.pop();
                op->undo(ctx);
            }
        }

        void
        moveStackToVec(std::stack<std::unique_ptr<TransactionOp>> &opsStack) {
            while (!opsStack.empty()) {
                m_ops.push_back(std::move(opsStack.top()));
                opsStack.pop();
            }
        }

      private:
        std::vector<std::unique_ptr<TransactionOp>> m_ops;
    };

    typedef std::function<void(TransactionCtx &)> TTransactionFn;
} // namespace Bess::Session
