#include "ui/ui_main/dialogs.h"
#include "tinyfiledialogs.h"

namespace Bess::UI {
    constexpr const char *filterList[1] = {"*.bproj"};

    std::string Dialogs::showSaveFileDialog(const std::string &title, const std::string &filters) {
        const auto filepath = tinyfd_saveFileDialog(title.c_str(),
                                                    "",
                                                    1,
                                                    filterList,
                                                    "Bess Project");

        if (filepath == nullptr)
            return "";

        return filepath;
    }

    std::string Dialogs::showSelectPathDialog(const std::string &title) {
        const auto filepath = tinyfd_selectFolderDialog("Select Location", "");

        if (filepath == nullptr)
            return "";

        return filepath;
    }

    std::string Dialogs::showOpenFileDialog(const std::string &title, const std::string &filters) {
        const auto filepath = tinyfd_openFileDialog(title.c_str(),
                                                    "",
                                                    1,
                                                    filterList,
                                                    "Bess Project",
                                                    false);

        if (filepath == nullptr)
            return "";

        return filepath;
    }
} // namespace Bess::UI
