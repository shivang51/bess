#pragma once

#include <string>


namespace Bess::UI {
    class Dialogs {
    public:
        static std::string showSaveFileDialog(const std::string& title, const std::string& filters);
        static std::string showOpenFileDialog(const std::string& title, const std::string& filters);
    };
}