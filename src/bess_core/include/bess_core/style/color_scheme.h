#pragma once

#include "common/bess_api.h"

// Just trying to create something like Flutter
// https://github.com/flutter/flutter/blob/master/packages/flutter/lib/src/material/color_scheme.dart
#include "bess_core/renderer/renderer_types.h"
#include "common/class_helpers.h"
#include "cpp/dynamiccolor/dynamic_scheme.h"
#include "json/value.h"
#include <cstdint>

namespace Bess::Core::Style {
    enum class Brightness : uint8_t { light, dark };

    using Color = Renderer::Color;
    using DynamicScheme = material_color_utilities::DynamicScheme;

    struct BESS_API ColorSchemeColors {
        Color primary;
        Color onPrimary;
        Color primaryContainer;
        Color onPrimaryContainer;
        Color primaryFixed;
        Color primaryFixedDim;
        Color onPrimaryFixed;
        Color onPrimaryFixedVariant;

        Color secondary;
        Color onSecondary;
        Color secondaryContainer;
        Color onSecondaryContainer;
        Color secondaryFixed;
        Color secondaryFixedDim;
        Color onSecondaryFixed;
        Color onSecondaryFixedVariant;

        Color tertiary;
        Color onTertiary;
        Color tertiaryContainer;
        Color onTertiaryContainer;
        Color tertiaryFixed;
        Color tertiaryFixedDim;
        Color onTertiaryFixed;
        Color onTertiaryFixedVariant;

        Color error;
        Color onError;
        Color errorContainer;
        Color onErrorContainer;

        Color outline;
        Color outlineVariant;

        Color surface;
        Color onSurface;
        Color surfaceDim;
        Color surfaceBright;
        Color surfaceContainerLowest;
        Color surfaceContainerLow;
        Color surfaceContainer;
        Color surfaceContainerHigh;
        Color surfaceContainerHighest;
        Color onSurfaceVariant;

        Color inverseSurface;
        Color onInverseSurface;
        Color inversePrimary;

        Color shadow;
        Color scrim;
        Color surfaceTint;

        static ColorSchemeColors
        fromDynamicScheme(const DynamicScheme &dynamicScheme);
    };

    class BESS_API ColorScheme {
      public:
        constexpr ColorScheme() = default;
        constexpr ColorScheme(const ColorScheme &) = default;
        constexpr ColorScheme(ColorScheme &&) = default;
        ~ColorScheme() = default;

        constexpr ColorScheme &operator=(const ColorScheme &) = default;
        constexpr ColorScheme &operator=(ColorScheme &&) = default;

        ColorScheme(const ColorSchemeColors &colors, Brightness brightness);

        static constexpr ColorScheme
        fromSeed(const Renderer::Color &seedColor,
                 Brightness brightness = Brightness::dark) {
            ColorScheme scheme;
            scheme.m_brightness = brightness;

            const auto dynamicScheme = buildDynamicScheme(
                seedColor, brightness, scheme.m_contrastLevel);

            scheme.m_colors =
                ColorSchemeColors::fromDynamicScheme(dynamicScheme);

            return scheme;
        }

        MAKE_GETTER_SETTER(Brightness, Brightness, m_brightness);
        MAKE_GETTER_SETTER(ColorSchemeColors, Colors, m_colors);

        Json::Value toJson() const;
        static ColorScheme fromJson(const Json::Value &json);

        bool isDark() const;

      private:
        static DynamicScheme
        buildDynamicScheme(const Renderer::Color &seedColor,
                           Brightness brightness,
                           float contrast);

      private:
        ColorSchemeColors m_colors;
        Brightness m_brightness{Brightness::dark};
        float m_contrastLevel{0.0f};
    };

} // namespace Bess::Core::Style
