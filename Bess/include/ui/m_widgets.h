#pragma once

#include <string>
#include <vector>

namespace Bess::UI {
    class MWidgets {
    public:
        static bool TextBox(const std::string& label, std::string& value, const std::string& hintText = "");
        static bool ComboBox(const std::string& label, std::string& currentValue, const std::vector<std::string>& predefinedValues);
        static bool ComboBox(const std::string& label, float& currentValue, const std::vector<float>& predefinedValues);
        static bool ComboBox(const std::string& label, int& currentValue, const std::vector<int>& predefinedValues);
    };
}