#pragma once

#include <string>
#include <vector>

namespace Bess::UI {
    class Dialogs {
      public:
        static std::string showSaveFileDialog(const std::string &title, const std::string &filters);
        static std::string showOpenFileDialog(const std::string &title, const std::string &filters);
        static std::string showSelectPathDialog(const std::string &title);
    };
} // namespace Bess::UI
