#pragma once

#include "common/helpers.h"
#include "imgui.h"
#include "ui/icons/CodIcons.h"
#include "ui/ui_main/project_explorer_state.h"
#include <cstdint>
#include <string>
#include <vector>

namespace Bess::UI {
    class ProjectExplorer {
      public:
        static constexpr auto windowName = Common::Helpers::concat(
            Icons::CodIcons::SYMBOL_CLASS, "  Project Explorer");

        static void draw();

        static bool isShown;
        static ProjectExplorerState state;

      private:
        static std::pair<bool, bool> ProjectExplorerNode(int key, uint64_t nodeId,
                                                         const char *label,
                                                         bool selected,
                                                         bool multiSelectMode);

        static bool EditableTreeNode(uint64_t key,
                                     std::string &name,
                                     bool &selected,
                                     ImGuiTreeNodeFlags treeFlags,
                                     const std::string &icon,
                                     glm::vec4 iconColor = glm::vec4(-1.0f),
                                     const std::string &popupName = "");
        static void firstTime();
        static size_t drawNodes(std::vector<std::shared_ptr<UI::ProjectExplorerNode>> &nodes,
                                bool isRoot = false);

        static bool isfirstTimeDraw;
        static ImColor itemAltBg;
    };
} // namespace Bess::UI
