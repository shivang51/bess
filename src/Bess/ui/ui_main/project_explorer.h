#pragma once

#include "common/helpers.h"
#include "imgui.h"
#include "ui/icons/CodIcons.h"
#include <string>

namespace Bess::UI {
    class ProjectExplorer {
      public:
        static constexpr auto windowName = Common::Helpers::concat(
            Icons::CodIcons::SYMBOL_CLASS, "  Project Explorer");

        static void draw();

        static bool isShown;

        static std::pair<bool, bool> ProjectExplorerNode(int key, uint64_t nodeId,
                                                         const char *label,
                                                         bool selected,
                                                         bool multiSelectMode);

        static bool EditableTreeNode(int key,
                                     std::string &name,
                                     bool &selected,
                                     ImGuiTreeNodeFlags treeFlags,
                                     const std::string &icon,
                                     glm::vec4 iconColor = glm::vec4(-1.0f));

      private:
        static void firstTime();
        static bool isfirstTimeDraw;
        static ImColor itemAltBg;
    };
} // namespace Bess::UI
