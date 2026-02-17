#pragma once

#include "common/helpers.h"
#include "scene/scene_events.h"
#include "ui/icons/CodIcons.h"
#include "ui/ui_main/project_explorer_state.h"
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace Bess::UI {
    class ProjectExplorer {
      public:
        static constexpr auto windowName = Common::Helpers::concat(
            Icons::CodIcons::SYMBOL_CLASS, "  Project Explorer");

        static void init(); // New init method
        static void draw();

        static bool isShown;
        static ProjectExplorerState state;

        static void groupSelectedNodes();
        static void groupOnNets();

      private:
        static void onEntityCreated(const Bess::Canvas::Events::ComponentAddedEvent &e);
        static void onEntityDestroyed(const Bess::Canvas::Events::ComponentRemovedEvent &e);

        static std::pair<bool, bool> drawLeafNode(size_t key, uint64_t nodeId,
                                                  const char *label,
                                                  bool selected,
                                                  bool multiSelectMode);

        static void firstTime();
        static size_t drawNodes(std::vector<std::shared_ptr<UI::ProjectExplorerNode>> &nodes,
                                bool isRoot = false);

        static size_t drawEntites(const std::unordered_set<UUID> &entities);

        static void clearAllSelections();
        static void selectRange(int start, int end);
        static void selectNode(const std::shared_ptr<UI::ProjectExplorerNode> &node);
        static void deleteNode(const std::shared_ptr<UI::ProjectExplorerNode> &node, bool firstCall = true);

        static bool isfirstTimeDraw;
        static int32_t lastSelectedIndex;
        static size_t nodesKeyCounter;
    };
} // namespace Bess::UI
