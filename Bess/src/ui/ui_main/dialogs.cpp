#include "ui/ui_main/dialogs.h"
#include "tinyfiledialogs.h"

namespace Bess::UI {
    std::vector<std::string> Dialogs::filterList = {"*.bproj"};

    std::string Dialogs::showSaveFileDialog(const std::string &title, const std::string &filters) {

        auto filepath = tinyfd_saveFileDialog("Open Bess Project", "", filterList.size(),
                                              (const char *const *)filterList.data(), "Bess Project");

        return filepath;
    }

    std::string Dialogs::showOpenFileDialog(const std::string &title, const std::string &filters) {
        auto filepath = tinyfd_openFileDialog("Open Bess Project", "", filterList.size(),
                                              (const char *const *)filterList.data(), "Bess Project", false);

        return filepath;
    }
} // namespace Bess::UI
