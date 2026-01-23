#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <variant>
#include <vector>

namespace Bess::UI::Hook {
    enum class PropertyDescType : uint8_t {
        bool_t,
        int_t,
        float_t,
        string_t,
        enum_t
    };

    using PropertyValue = std::variant<
        bool,
        int64_t,
        double,
        std::string>;

    struct NumericConstraints {
        double min;
        double max;
        double step;
    };

    struct EnumConstraints {
        std::vector<std::string> labels;
        std::vector<int> values;
    };

    struct PropertyBinding {
        std::function<PropertyValue()> getter;
        std::function<void(const PropertyValue &)> setter;
    };

    using PropertyConstraints = std::variant<
        std::monostate,
        NumericConstraints,
        EnumConstraints>;

    struct PropertyDesc {
        std::string name;
        PropertyDescType type;
        PropertyValue defaultValue;
        PropertyConstraints constraints;
        PropertyBinding binding;
    };

    class UIHook {
      public:
        virtual ~UIHook() = default;

        virtual void setPropertyDescriptors(const std::vector<PropertyDesc> &descs) {
            m_propDescriptors = descs;
        }

        virtual const std::vector<PropertyDesc> &getPropertyDescriptors() const {
            return m_propDescriptors;
        }

        virtual void addPropertyDescriptor(const PropertyDesc &desc) {
            m_propDescriptors.push_back(desc);
        }

      private:
        std::vector<PropertyDesc> m_propDescriptors;
    };

} // namespace Bess::UI::Hook
