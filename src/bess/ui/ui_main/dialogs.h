#pragma once

#include <string>
#include <vector>

namespace Bess::UI {
    class Dialogs {
      public:
        typedef std::vector<std::string> FilterMap;

        static std::string showSaveFileDialog(const std::string &title,
                                              const FilterMap &filters);
        static std::string showOpenFileDialog(const std::string &title,
                                              const FilterMap &filters);
        static std::vector<std::string> showOpenFilesDialog(const std::string &title,
                                  const FilterMap &filters);
        static std::string showSelectPathDialog(const std::string &title);
    };
} // namespace Bess::UI
