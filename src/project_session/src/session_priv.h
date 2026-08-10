#pragma once

#include "project_session/project_session.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace Bess {
    class ProjectSessionStep {
      public:
        virtual ~ProjectSessionStep() = default;

        [[nodiscard]] virtual Status apply(ProjectSession &session) = 0;
        [[nodiscard]] virtual Status undo(ProjectSession &session) = 0;
        [[nodiscard]] virtual Status redo(ProjectSession &session) {
            return apply(session);
        }

        [[nodiscard]] virtual bool merge(const ProjectSessionStep &next) {
            (void)next;
            return false;
        }

        [[nodiscard]] virtual std::size_t bytes() const noexcept {
            return sizeof(*this);
        }

      protected:
        static void putName(ProjectSession &session, std::string name);
    };

    struct HistEntry {
        std::string name;
        std::vector<std::unique_ptr<ProjectSessionStep>> ops;
        StateId before = 0;
        StateId after = 0;
        std::size_t bytes = 0;
    };

    class ProjectTx::Impl {
      public:
        ProjectSession *session = nullptr;
        std::string name;
        TxOpts opts;
        StateId base = 0;
        std::vector<std::unique_ptr<ProjectSessionStep>> ops;
        std::unordered_set<UUID> adds;
        Status err;
        bool done = false;
    };
} // namespace Bess
