#include "ui/ui_main/dialogs.h"
#include "portable-file-dialogs.h"
#include <vector>

namespace Bess::UI {
    std::string Dialogs::showSaveFileDialog(const std::string &title, const FilterMap &filters) {
        const auto filepath = pfd::save_file(title,
                                             "",
                                             filters)
                                  .result();

        return filepath;
    }

    std::string Dialogs::showSelectPathDialog(const std::string &title) {
        const auto filepath = pfd::select_folder("Select Location", "").result();
        return filepath;
    }

    std::string Dialogs::showOpenFileDialog(const std::string &title, const FilterMap &filters) {
        auto selection = pfd::open_file(title,
                                        "",
                                        filters)
                             .result();

        return selection.empty() ? "" : selection.front();
    }
} // namespace Bess::UI
