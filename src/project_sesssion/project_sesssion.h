#pragma once
#include "common/bess_api.h"
#include "common/class_helpers.h"

#include "transaction.h"
#include <memory>

namespace Bess::Session {
    class ProjectDocument;

    class BESS_API ProjectSession {
      public:
        bool transact(const TTransactionFn &func) {
            TransactionCtx ctx;

            func(ctx);

            const bool res = execTxn(ctx);

            if (res) {
                m_saved = false;
            }

            return res;
        }

        bool undo() {
            TxnExecCtx ctx;

            auto txn = std::move(m_undoStack.top());
            m_undoStack.pop();

            if (txn.undo(ctx)) {
                m_redoStack.push(txn);
                m_saved = false;
                return true;
            }

            return false;
        }

        bool redo() {
            TxnExecCtx ctx;

            auto txn = std::move(m_redoStack.top());
            m_redoStack.pop();

            if (txn.redo(ctx)) {
                m_undoStack.push(txn);
                m_saved = false;
                return true;
            }

            return false;
        }

        bool saveProject() {
            m_saved = true;
            return true;
        }

      private:
        bool execTxn(TransactionCtx &txnCtx) {
            TxnExecCtx execCtx;
            if (txnCtx.execute(execCtx)) {
                m_undoStack.push(std::move(txnCtx));
            }
            return false;
        }

      private:
        std::unique_ptr<ProjectDocument> m_projectDoc;
        bool m_saved = false;

        std::stack<TransactionCtx> m_undoStack;
        std::stack<TransactionCtx> m_redoStack;
    };
} // namespace Bess::Session
