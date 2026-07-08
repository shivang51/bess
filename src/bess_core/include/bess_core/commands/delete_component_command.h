#pragma once

#include "bess_core/commands/command.h"
#include "bess_core/commands/scene_component_command_hooks.h"
#include "bess_core/scene/scene_state/components/scene_component.h"
#include "common/bess_api.h"
#include "common/bess_uuid.h"
#include <functional>
#include <memory>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Bess::Cmd {
    using DeleteCompCmdCB = std::function<void(
        bool, const std::vector<std::shared_ptr<Canvas::SceneComponent>> &)>;

    class BESS_API DeleteCompCmd : public Command {
      public:
        DeleteCompCmd();

        explicit DeleteCompCmd(const std::vector<UUID> &componentUuids,
                               DeleteCompCmdCB callback = nullptr);

        bool execute(const CommandContext &context) override;
        void undo(const CommandContext &context) override;
        void redo(const CommandContext &context) override;

        const std::vector<std::shared_ptr<Canvas::SceneComponent>> &
        getDeletedComponents() const;

      private:
        std::vector<UUID>
        buildDeletionOrder(const CommandContext &context) const;
        void captureChildComponentSnapshots(
            const CommandContext &context,
            const std::vector<UUID> &deletionOrder);
        bool removeStoredComponents(const CommandContext &context);
        void restoreStoredComponents(const CommandContext &context);
        void restoreChildComponentSnapshots(
            Canvas::SceneState &sceneState) const;

        std::set<UUID> m_componentUuids;
        std::vector<std::shared_ptr<Canvas::SceneComponent>>
            m_deletedComponents;
        std::unordered_map<UUID, OrderedSet<UUID>> m_childComponentSnapshots;
        DeleteCompCmdCB m_callback = nullptr;
    };
} // namespace Bess::Cmd
