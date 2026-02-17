#pragma once

#include "bess_uuid.h"
#include "common/helpers.h"
#include "ui/icons/CodIcons.h"
#include <cstdint>
#include <unordered_set>

namespace Bess::UI {
    class ProjectExplorer {
      public:
        static constexpr auto windowName = Common::Helpers::concat(
            Icons::CodIcons::SYMBOL_CLASS, "  Project Explorer");

        static void init(); // New init method
        static void draw();

        static bool isShown;

        static void groupSelectedNodes();
        static void groupOnNets();

      private:
        static std::pair<bool, bool> drawLeafNode(size_t key, uint64_t nodeId,
                                                  const char *label,
                                                  bool selected,
                                                  bool multiSelectMode);

        static void firstTime();

        static size_t drawEntites(const std::unordered_set<UUID> &entities);

        static bool isfirstTimeDraw;
        static int32_t lastSelectedIndex;
        static size_t nodesKeyCounter;
    };
} // namespace Bess::UI
