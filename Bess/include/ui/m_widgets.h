#pragma once

#include <string>


namespace Bess::UI {
    class MWidgets {
    public:
        static bool TextBox(const std::string& label, std::string& value, const std::string& hintText = "");
    };
}