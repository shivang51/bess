
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#endif

#include "portable-file-dialogs.h"
#include "ui/ui_main/dialogs.h"
#include <vector>

namespace Bess::UI {
    std::string Dialogs::showSaveFileDialog(const std::string &title,
                                            const FilterMap &filters) {
        const auto filepath = pfd::save_file(title, "", filters).result();

        return filepath;
    }

    std::string Dialogs::showSelectPathDialog(const std::string &title) {
        const auto filepath =
            pfd::select_folder("Select Location", "").result();
        return filepath;
    }

    std::string Dialogs::showOpenFileDialog(const std::string &title,
                                            const FilterMap &filters) {
        auto selection = pfd::open_file(title, "", filters).result();

        return selection.empty() ? "" : selection.front();
    }

    std::vector<std::string>
    Dialogs::showOpenFilesDialog(const std::string &title,
                                 const FilterMap &filters) {
        return pfd::open_file(title, "", filters, pfd::opt::multiselect)
            .result();
    }
} // namespace Bess::UI
