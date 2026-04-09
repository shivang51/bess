#pragma once

#include "common/bess_uuid.h"
#include "ui_panel.h"
#include <cstdint>
#include <string>
#include <unordered_set>

namespace Bess::UI {
    class ProjectExplorer : public Panel {
      public:
        ProjectExplorer();

        void onDraw() override;

        void groupSelectedNodes();
        void groupOnNets();

      private:
        static bool drawLeafNode(size_t key, uint64_t nodeId,
                                 const char *label,
                                 bool selected,
                                 bool multiSelectMode);

        bool shouldDisplayEntity(const UUID &entityId) const;
        size_t drawEntites(const std::unordered_set<UUID> &entities);

        int32_t m_lastSelectedIndex;
        size_t m_nodesKeyCounter;
        std::string m_searchQuery;
        bool m_filterInputs = false;
        bool m_filterOutputs = false;
    };
} // namespace Bess::UI
