#pragma once

#include "project_session/status.h"

#include "common/bess_uuid.h"
#include "fwd.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Bess {
    class ProjectSession;

    namespace Canvas {
        class ConnectionSceneComponent;
        class SceneComponent;
        class SlotSceneComponent;
    } // namespace Canvas

    namespace SimEngine::Drivers {
        class CompDef;
    }

    struct TxOpts {
        bool rebase = false;
        bool empty = false;
        bool hist = true;
    };

    class ProjectTx {
      public:
        using Act = std::move_only_function<Status(ProjectSession &)>;

        ProjectTx(const ProjectTx &) = delete;
        ProjectTx &operator=(const ProjectTx &) = delete;
        ProjectTx(ProjectTx &&) noexcept;
        ProjectTx &operator=(ProjectTx &&) noexcept;
        ~ProjectTx();

        [[nodiscard]] Status
        addComp(std::shared_ptr<Canvas::SceneComponent> comp,
                std::vector<std::shared_ptr<Canvas::SceneComponent>> kids = {},
                UUID scene = UUID::null);

        // Records a component graph that has already been added.
        [[nodiscard]] Status
        trackAdd(std::shared_ptr<Canvas::SceneComponent> comp,
                 std::vector<std::shared_ptr<Canvas::SceneComponent>> kids = {},
                 UUID scene = UUID::null);

        [[nodiscard]] Status rmComp(std::vector<UUID> ids,
                                    UUID scene = UUID::null);
        [[nodiscard]] Status rmComp(UUID id, UUID scene = UUID::null);

        [[nodiscard]] Status
        addSlot(std::shared_ptr<Canvas::SlotSceneComponent> slot,
                UUID parent,
                UUID scene = UUID::null);

        [[nodiscard]] Status
        addConn(std::shared_ptr<Canvas::ConnectionSceneComponent> conn,
                UUID scene = UUID::null);

        [[nodiscard]] Status
        trackConn(std::shared_ptr<Canvas::ConnectionSceneComponent> conn,
                  UUID scene = UUID::null);

        [[nodiscard]] Status
        moveComp(UUID id, glm::vec3 pos, UUID scene = UUID::null);
        [[nodiscard]] Status trackMove(UUID id,
                                       glm::vec3 from,
                                       glm::vec3 to,
                                       UUID scene = UUID::null);

        [[nodiscard]] Status
        parentComp(UUID id, UUID parent, UUID scene = UUID::null);
        [[nodiscard]] Status
        trackParent(UUID id, UUID from, UUID to, UUID scene = UUID::null);

        // Escape hatch for Bess features whose state spans several domains.
        [[nodiscard]] Status step(std::string name,
                                  Act apply,
                                  Act undo,
                                  Act redo = {},
                                  std::size_t bytes = 0);

        [[nodiscard]] TxResult commit();
        void cancel() noexcept;

        [[nodiscard]] bool isEmpty() const noexcept;
        [[nodiscard]] bool done() const noexcept;
        [[nodiscard]] std::string_view name() const noexcept;
        [[nodiscard]] StateId base() const noexcept;

      private:
        friend class ProjectSession;
        class Impl;

        ProjectTx(ProjectSession &session,
                  std::string name,
                  TxOpts opts,
                  StateId base);
        [[nodiscard]] Status bad(Status status);

        std::unique_ptr<Impl> m;
    };
} // namespace Bess
