#include "ui/ui_main/dialogs.h"
#include "tinyfiledialogs.h"

namespace Bess::UI {
    const char* filterList[1] = {"*.bproj"};

    std::string Dialogs::showSaveFileDialog(const std::string &title, const std::string &filters) {

        const auto filepath = tinyfd_saveFileDialog("Open Bess Project", "", 1, filterList, "Bess Project");

        if (filepath == NULL)
            return "";

        return filepath;
    }

    std::string Dialogs::showSelectPathDialog(const std::string &title) {
        const auto filepath = tinyfd_selectFolderDialog("Select Location", "");

        if (filepath == NULL)
            return "";

        return filepath;
    }

    std::string Dialogs::showOpenFileDialog(const std::string &title, const std::string &filters) {
        const auto filepath = tinyfd_openFileDialog("Open Bess Project", "", 1, filterList, "Bess Project", false);

        if (filepath == NULL)
            return "";

        return filepath;
    }
} // namespace Bess::UI
