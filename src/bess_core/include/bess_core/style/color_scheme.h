#pragma once

// Just trying to create something like Flutter
// https://github.com/flutter/flutter/blob/master/packages/flutter/lib/src/material/color_scheme.dart
#include "bess_core/renderer/renderer_types.h"
#include "color_utils.h"
#include "common/bess_assert.h"
#include "common/class_helpers.h"
#include "cpp/cam/hct.h"
#include <cstdint>

namespace Bess::Core::Style {
    enum class Brightness : uint8_t { light, dark };

    using Color = Renderer::Color;

    struct ColorSchemeColors {
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
    };

    class ColorScheme {
      public:
        constexpr ColorScheme() = default;
        constexpr ColorScheme(const ColorScheme &) = default;
        constexpr ColorScheme(ColorScheme &&) = default;
        ~ColorScheme() = default;

        constexpr ColorScheme &operator=(const ColorScheme &) = default;
        constexpr ColorScheme &operator=(ColorScheme &&) = default;

        static constexpr ColorScheme
        fromSeed(const Renderer::Color &seedColor,
                 Brightness brightness = Brightness::dark) {
            ColorScheme scheme;
            scheme.m_brightness = brightness;
            return scheme;
        }

        MAKE_GETTER_SETTER(Brightness, Brightness, m_brightness);

      private:
        static ColorScheme buildDynamicScheme(const Renderer::Color &seedColor,
                                              const Brightness brightness,
                                              const float contrast) {

            BESS_ASSERT(contrast >= -1.0f && contrast <= 1.0f,
                        "Contrast must be between -1.0 and 1.0");

            const bool isDark = brightness == Brightness::dark;

            const material_color_utilities::Hct sourceCol(seedColor.toARGB8());

            ColorScheme scheme;

            return scheme;
        }

      private:
        Brightness m_brightness{Brightness::dark};
        float m_contrastLevel{0.0f};
    };

} // namespace Bess::Core::Style
