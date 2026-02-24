#pragma once

#include "common/bess_uuid.h"
#include "ui_panel.h"
#include <cstdint>
#include <unordered_set>

namespace Bess::UI {
    class ProjectExplorer : public Panel {
      public:
        ProjectExplorer();

        void onDraw() override;

        void groupSelectedNodes();
        void groupOnNets();

      private:
        static std::pair<bool, bool> drawLeafNode(size_t key, uint64_t nodeId,
                                                  const char *label,
                                                  bool selected,
                                                  bool multiSelectMode);

        size_t drawEntites(const std::unordered_set<UUID> &entities);

        int32_t m_lastSelectedIndex;
        size_t m_nodesKeyCounter;
    };
} // namespace Bess::UI
