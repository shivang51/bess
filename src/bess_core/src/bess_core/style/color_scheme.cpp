#include "bess_core/style/color_scheme.h"

namespace Bess::Core::Style {
    ColorSchemeColors
    ColorSchemeColors::fromDynamicScheme(const DynamicScheme &dynamicScheme) {
        ColorSchemeColors colors;
        colors.primary = Color::FromARGB(dynamicScheme.GetPrimary());
        colors.onPrimary = Color::FromARGB(dynamicScheme.GetOnPrimary());
        colors.primaryContainer =
            Color::FromARGB(dynamicScheme.GetPrimaryContainer());
        colors.onPrimaryContainer =
            Color::FromARGB(dynamicScheme.GetOnPrimaryContainer());
        colors.primaryFixed = Color::FromARGB(dynamicScheme.GetPrimaryFixed());
        colors.primaryFixedDim =
            Color::FromARGB(dynamicScheme.GetPrimaryFixedDim());
        colors.onPrimaryFixed =
            Color::FromARGB(dynamicScheme.GetOnPrimaryFixed());
        colors.onPrimaryFixedVariant =
            Color::FromARGB(dynamicScheme.GetOnPrimaryFixedVariant());

        colors.secondary = Color::FromARGB(dynamicScheme.GetSecondary());
        colors.onSecondary = Color::FromARGB(dynamicScheme.GetOnSecondary());
        colors.secondaryContainer =
            Color::FromARGB(dynamicScheme.GetSecondaryContainer());
        colors.onSecondaryContainer =
            Color::FromARGB(dynamicScheme.GetOnSecondaryContainer());
        colors.secondaryFixed =
            Color::FromARGB(dynamicScheme.GetSecondaryFixed());
        colors.secondaryFixedDim =
            Color::FromARGB(dynamicScheme.GetSecondaryFixedDim());
        colors.onSecondaryFixed =
            Color::FromARGB(dynamicScheme.GetOnSecondaryFixed());
        colors.onSecondaryFixedVariant =
            Color::FromARGB(dynamicScheme.GetOnSecondaryFixedVariant());

        colors.tertiary = Color::FromARGB(dynamicScheme.GetTertiary());
        colors.onTertiary = Color::FromARGB(dynamicScheme.GetOnTertiary());
        colors.tertiaryContainer =
            Color::FromARGB(dynamicScheme.GetTertiaryContainer());
        colors.onTertiaryContainer =
            Color::FromARGB(dynamicScheme.GetOnTertiaryContainer());
        colors.tertiaryFixed =
            Color::FromARGB(dynamicScheme.GetTertiaryFixed());
        colors.tertiaryFixedDim =
            Color::FromARGB(dynamicScheme.GetTertiaryFixedDim());
        colors.onTertiaryFixed =
            Color::FromARGB(dynamicScheme.GetOnTertiaryFixed());
        colors.onTertiaryFixedVariant =
            Color::FromARGB(dynamicScheme.GetOnTertiaryFixedVariant());

        colors.error = Color::FromARGB(dynamicScheme.GetError());
        colors.onError = Color::FromARGB(dynamicScheme.GetOnError());
        colors.errorContainer =
            Color::FromARGB(dynamicScheme.GetErrorContainer());
        colors.onErrorContainer =
            Color::FromARGB(dynamicScheme.GetOnErrorContainer());

        colors.outline = Color::FromARGB(dynamicScheme.GetOutline());
        colors.outlineVariant =
            Color::FromARGB(dynamicScheme.GetOutlineVariant());

        colors.surface = Color::FromARGB(dynamicScheme.GetSurface());
        colors.onSurface = Color::FromARGB(dynamicScheme.GetOnSurface());
        colors.surfaceDim = Color::FromARGB(dynamicScheme.GetSurfaceDim());
        colors.surfaceBright =
            Color::FromARGB(dynamicScheme.GetSurfaceBright());
        colors.surfaceContainerLowest =
            Color::FromARGB(dynamicScheme.GetSurfaceContainerLowest());
        colors.surfaceContainerLow =
            Color::FromARGB(dynamicScheme.GetSurfaceContainerLow());
        colors.surfaceContainer =
            Color::FromARGB(dynamicScheme.GetSurfaceContainer());
        colors.surfaceContainerHigh =
            Color::FromARGB(dynamicScheme.GetSurfaceContainerHigh());
        colors.surfaceContainerHighest =
            Color::FromARGB(dynamicScheme.GetSurfaceContainerHighest());
        colors.onSurfaceVariant =
            Color::FromARGB(dynamicScheme.GetOnSurfaceVariant());

        colors.inverseSurface =
            Color::FromARGB(dynamicScheme.GetInverseSurface());
        colors.onInverseSurface =
            Color::FromARGB(dynamicScheme.GetInverseOnSurface());
        colors.inversePrimary =
            Color::FromARGB(dynamicScheme.GetInversePrimary());

        colors.shadow = Color::FromARGB(dynamicScheme.GetShadow());
        colors.scrim = Color::FromARGB(dynamicScheme.GetScrim());
        colors.surfaceTint = Color::FromARGB(dynamicScheme.GetSurfaceTint());

        return colors;
    }

} // namespace Bess::Core::Style
