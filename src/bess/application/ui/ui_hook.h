#pragma once

#include "common/bess_api.h"

#include "glm.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace Bess::UI::Hook {
    enum class PropertyDescType : uint8_t {
        bool_t,
        int_t,
        uint_t,
        float_t,
        string_t,
        enum_t,
        color_t
    };

    using PropertyValue =
        std::variant<bool, int64_t, uint64_t, double, std::string, glm::vec4>;

    struct BESS_API NumericConstraints {
        double min;
        double max;
        double step;
    };

    struct BESS_API EnumConstraints {
        std::vector<std::string> labels;
        std::vector<int> values;
    };

    struct BESS_API PropertyBinding {
        std::function<PropertyValue()> getter;
        std::function<void(const PropertyValue &)> setter;
    };

    using PropertyConstraints =
        std::variant<std::monostate, NumericConstraints, EnumConstraints>;

    struct BESS_API PropertyDesc {
        std::string name;
        PropertyDescType type;
        PropertyValue defaultValue;
        PropertyConstraints constraints;
        PropertyBinding binding;
    };

    class BESS_API UIHook {
      public:
        virtual ~UIHook() = default;

        virtual void
        setPropertyDescriptors(const std::vector<PropertyDesc> &descs);

        virtual const std::vector<PropertyDesc> &getPropertyDescriptors() const;

        virtual void addPropertyDescriptor(const PropertyDesc &desc);

        virtual void draw();

      private:
        void drawProperty(const PropertyDesc &desc);

      private:
        std::vector<PropertyDesc> m_propDescriptors;
        std::unordered_map<std::string, std::string> m_stringPool;
        std::unordered_map<std::string, std::vector<std::string>> m_enumPool;
    };

} // namespace Bess::UI::Hook
