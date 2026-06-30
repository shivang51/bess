#include "bess_core/style/color_scheme.h"
#include "common/bess_assert.h"
#include "cpp/cam/hct.h"
#include "cpp/scheme/scheme_neutral.h"

namespace Bess::Core::Style {
    ColorSchemeColors
    ColorSchemeColors::fromDynamicScheme(const DynamicScheme &dynamicScheme) {
        ColorSchemeColors colors;
        colors.primary = Color::fromARGB(dynamicScheme.GetPrimary());
        colors.onPrimary = Color::fromARGB(dynamicScheme.GetOnPrimary());
        colors.primaryContainer =
            Color::fromARGB(dynamicScheme.GetPrimaryContainer());
        colors.onPrimaryContainer =
            Color::fromARGB(dynamicScheme.GetOnPrimaryContainer());
        colors.primaryFixed = Color::fromARGB(dynamicScheme.GetPrimaryFixed());
        colors.primaryFixedDim =
            Color::fromARGB(dynamicScheme.GetPrimaryFixedDim());
        colors.onPrimaryFixed =
            Color::fromARGB(dynamicScheme.GetOnPrimaryFixed());
        colors.onPrimaryFixedVariant =
            Color::fromARGB(dynamicScheme.GetOnPrimaryFixedVariant());

        colors.secondary = Color::fromARGB(dynamicScheme.GetSecondary());
        colors.onSecondary = Color::fromARGB(dynamicScheme.GetOnSecondary());
        colors.secondaryContainer =
            Color::fromARGB(dynamicScheme.GetSecondaryContainer());
        colors.onSecondaryContainer =
            Color::fromARGB(dynamicScheme.GetOnSecondaryContainer());
        colors.secondaryFixed =
            Color::fromARGB(dynamicScheme.GetSecondaryFixed());
        colors.secondaryFixedDim =
            Color::fromARGB(dynamicScheme.GetSecondaryFixedDim());
        colors.onSecondaryFixed =
            Color::fromARGB(dynamicScheme.GetOnSecondaryFixed());
        colors.onSecondaryFixedVariant =
            Color::fromARGB(dynamicScheme.GetOnSecondaryFixedVariant());

        colors.tertiary = Color::fromARGB(dynamicScheme.GetTertiary());
        colors.onTertiary = Color::fromARGB(dynamicScheme.GetOnTertiary());
        colors.tertiaryContainer =
            Color::fromARGB(dynamicScheme.GetTertiaryContainer());
        colors.onTertiaryContainer =
            Color::fromARGB(dynamicScheme.GetOnTertiaryContainer());
        colors.tertiaryFixed =
            Color::fromARGB(dynamicScheme.GetTertiaryFixed());
        colors.tertiaryFixedDim =
            Color::fromARGB(dynamicScheme.GetTertiaryFixedDim());
        colors.onTertiaryFixed =
            Color::fromARGB(dynamicScheme.GetOnTertiaryFixed());
        colors.onTertiaryFixedVariant =
            Color::fromARGB(dynamicScheme.GetOnTertiaryFixedVariant());

        colors.error = Color::fromARGB(dynamicScheme.GetError());
        colors.onError = Color::fromARGB(dynamicScheme.GetOnError());
        colors.errorContainer =
            Color::fromARGB(dynamicScheme.GetErrorContainer());
        colors.onErrorContainer =
            Color::fromARGB(dynamicScheme.GetOnErrorContainer());

        colors.outline = Color::fromARGB(dynamicScheme.GetOutline());
        colors.outlineVariant =
            Color::fromARGB(dynamicScheme.GetOutlineVariant());

        colors.surface = Color::fromARGB(dynamicScheme.GetSurface());
        colors.onSurface = Color::fromARGB(dynamicScheme.GetOnSurface());
        colors.surfaceDim = Color::fromARGB(dynamicScheme.GetSurfaceDim());
        colors.surfaceBright =
            Color::fromARGB(dynamicScheme.GetSurfaceBright());
        colors.surfaceContainerLowest =
            Color::fromARGB(dynamicScheme.GetSurfaceContainerLowest());
        colors.surfaceContainerLow =
            Color::fromARGB(dynamicScheme.GetSurfaceContainerLow());
        colors.surfaceContainer =
            Color::fromARGB(dynamicScheme.GetSurfaceContainer());
        colors.surfaceContainerHigh =
            Color::fromARGB(dynamicScheme.GetSurfaceContainerHigh());
        colors.surfaceContainerHighest =
            Color::fromARGB(dynamicScheme.GetSurfaceContainerHighest());
        colors.onSurfaceVariant =
            Color::fromARGB(dynamicScheme.GetOnSurfaceVariant());

        colors.inverseSurface =
            Color::fromARGB(dynamicScheme.GetInverseSurface());
        colors.onInverseSurface =
            Color::fromARGB(dynamicScheme.GetInverseOnSurface());
        colors.inversePrimary =
            Color::fromARGB(dynamicScheme.GetInversePrimary());

        colors.shadow = Color::fromARGB(dynamicScheme.GetShadow());
        colors.scrim = Color::fromARGB(dynamicScheme.GetScrim());
        colors.surfaceTint = Color::fromARGB(dynamicScheme.GetSurfaceTint());

        return colors;
    }

    ColorScheme::ColorScheme(const ColorSchemeColors &colors,
                             Brightness brightness)
        : m_colors(colors),
          m_brightness(brightness) {
    }

    bool ColorScheme::isDark() const {
        return m_brightness == Brightness::dark;
    }

    Json::Value ColorScheme::toJson() const {
        Json::Value json;
        auto &colors = json["colors"];
        colors["primary"] = m_colors.primary.toJson();
        colors["onPrimary"] = m_colors.onPrimary.toJson();
        colors["primaryContainer"] = m_colors.primaryContainer.toJson();
        colors["onPrimaryContainer"] = m_colors.onPrimaryContainer.toJson();
        colors["primaryFixed"] = m_colors.primaryFixed.toJson();
        colors["primaryFixedDim"] = m_colors.primaryFixedDim.toJson();
        colors["onPrimaryFixed"] = m_colors.onPrimaryFixed.toJson();
        colors["onPrimaryFixedVariant"] =
            m_colors.onPrimaryFixedVariant.toJson();

        colors["secondary"] = m_colors.secondary.toJson();
        colors["onSecondary"] = m_colors.onSecondary.toJson();
        colors["secondaryContainer"] = m_colors.secondaryContainer.toJson();
        colors["onSecondaryContainer"] = m_colors.onSecondaryContainer.toJson();
        colors["secondaryFixed"] = m_colors.secondaryFixed.toJson();
        colors["secondaryFixedDim"] = m_colors.secondaryFixedDim.toJson();
        colors["onSecondaryFixed"] = m_colors.onSecondaryFixed.toJson();
        colors["onSecondaryFixedVariant"] =
            m_colors.onSecondaryFixedVariant.toJson();

        colors["tertiary"] = m_colors.tertiary.toJson();
        colors["onTertiary"] = m_colors.onTertiary.toJson();
        colors["tertiaryContainer"] = m_colors.tertiaryContainer.toJson();
        colors["onTertiaryContainer"] = m_colors.onTertiaryContainer.toJson();
        colors["tertiaryFixed"] = m_colors.tertiaryFixed.toJson();
        colors["tertiaryFixedDim"] = m_colors.tertiaryFixedDim.toJson();
        colors["onTertiaryFixed"] = m_colors.onTertiaryFixed.toJson();
        colors["onTertiaryFixedVariant"] =
            m_colors.onTertiaryFixedVariant.toJson();

        colors["error"] = m_colors.error.toJson();
        colors["onError"] = m_colors.onError.toJson();
        colors["errorContainer"] = m_colors.errorContainer.toJson();
        colors["onErrorContainer"] = m_colors.onErrorContainer.toJson();

        colors["outline"] = m_colors.outline.toJson();
        colors["outlineVariant"] = m_colors.outlineVariant.toJson();

        colors["surface"] = m_colors.surface.toJson();
        colors["onSurface"] = m_colors.onSurface.toJson();
        colors["surfaceDim"] = m_colors.surfaceDim.toJson();
        colors["surfaceBright"] = m_colors.surfaceBright.toJson();
        colors["surfaceContainerLowest"] =
            m_colors.surfaceContainerLowest.toJson();
        colors["surfaceContainerLow"] = m_colors.surfaceContainerLow.toJson();
        colors["surfaceContainer"] = m_colors.surfaceContainer.toJson();
        colors["surfaceContainerHigh"] = m_colors.surfaceContainerHigh.toJson();
        colors["surfaceContainerHighest"] =
            m_colors.surfaceContainerHighest.toJson();
        colors["onSurfaceVariant"] = m_colors.onSurfaceVariant.toJson();

        colors["inverseSurface"] = m_colors.inverseSurface.toJson();
        colors["onInverseSurface"] = m_colors.onInverseSurface.toJson();
        colors["inversePrimary"] = m_colors.inversePrimary.toJson();

        colors["shadow"] = m_colors.shadow.toJson();
        colors["scrim"] = m_colors.scrim.toJson();
        colors["surfaceTint"] = m_colors.surfaceTint.toJson();

        json["brightness"] =
            m_brightness == Brightness::light ? "light" : "dark";

        return json;
    }

    ColorScheme ColorScheme::fromJson(const Json::Value &json) {
        ColorSchemeColors colors;
        colors.primary = Color::fromJson(json["colors"]["primary"]);
        colors.onPrimary = Color::fromJson(json["colors"]["onPrimary"]);
        colors.primaryContainer =
            Color::fromJson(json["colors"]["primaryContainer"]);
        colors.onPrimaryContainer =
            Color::fromJson(json["colors"]["onPrimaryContainer"]);
        colors.primaryFixed = Color::fromJson(json["colors"]["primaryFixed"]);
        colors.primaryFixedDim =
            Color::fromJson(json["colors"]["primaryFixedDim"]);
        colors.onPrimaryFixed =
            Color::fromJson(json["colors"]["onPrimaryFixed"]);
        colors.onPrimaryFixedVariant =
            Color::fromJson(json["colors"]["onPrimaryFixedVariant"]);

        colors.secondary = Color::fromJson(json["colors"]["secondary"]);
        colors.onSecondary = Color::fromJson(json["colors"]["onSecondary"]);
        colors.secondaryContainer =
            Color::fromJson(json["colors"]["secondaryContainer"]);
        colors.onSecondaryContainer =
            Color::fromJson(json["colors"]["onSecondaryContainer"]);
        colors.secondaryFixed =
            Color::fromJson(json["colors"]["secondaryFixed"]);
        colors.secondaryFixedDim =
            Color::fromJson(json["colors"]["secondaryFixedDim"]);
        colors.onSecondaryFixed =
            Color::fromJson(json["colors"]["onSecondaryFixed"]);
        colors.onSecondaryFixedVariant =
            Color::fromJson(json["colors"]["onSecondaryFixedVariant"]);

        colors.tertiary = Color::fromJson(json["colors"]["tertiary"]);
        colors.onTertiary = Color::fromJson(json["colors"]["onTertiary"]);
        colors.tertiaryContainer =
            Color::fromJson(json["colors"]["tertiaryContainer"]);
        colors.onTertiaryContainer =
            Color::fromJson(json["colors"]["onTertiaryContainer"]);
        colors.tertiaryFixed = Color::fromJson(json["colors"]["tertiaryFixed"]);
        colors.tertiaryFixedDim =
            Color::fromJson(json["colors"]["tertiaryFixedDim"]);
        colors.onTertiaryFixed =
            Color::fromJson(json["colors"]["onTertiaryFixed"]);
        colors.onTertiaryFixedVariant =
            Color::fromJson(json["colors"]["onTertiaryFixedVariant"]);

        colors.error = Color::fromJson(json["colors"]["error"]);
        colors.onError = Color::fromJson(json["colors"]["onError"]);
        colors.errorContainer =
            Color::fromJson(json["colors"]["errorContainer"]);
        colors.onErrorContainer =
            Color::fromJson(json["colors"]["onErrorContainer"]);

        colors.outline = Color::fromJson(json["colors"]["outline"]);
        colors.outlineVariant =
            Color::fromJson(json["colors"]["outlineVariant"]);

        colors.surface = Color::fromJson(json["colors"]["surface"]);
        colors.onSurface = Color::fromJson(json["colors"]["onSurface"]);
        colors.surfaceDim = Color::fromJson(json["colors"]["surfaceDim"]);
        colors.surfaceBright = Color::fromJson(json["colors"]["surfaceBright"]);
        colors.surfaceContainerLowest =
            Color::fromJson(json["colors"]["surfaceContainerLowest"]);
        colors.surfaceContainerLow =
            Color::fromJson(json["colors"]["surfaceContainerLow"]);
        colors.surfaceContainer =
            Color::fromJson(json["colors"]["surfaceContainer"]);
        colors.surfaceContainerHigh =
            Color::fromJson(json["colors"]["surfaceContainerHigh"]);
        colors.surfaceContainerHighest =
            Color::fromJson(json["colors"]["surfaceContainerHighest"]);
        colors.onSurfaceVariant =
            Color::fromJson(json["colors"]["onSurfaceVariant"]);

        colors.inverseSurface =
            Color::fromJson(json["colors"]["inverseSurface"]);
        colors.onInverseSurface =
            Color::fromJson(json["colors"]["onInverseSurface"]);
        colors.inversePrimary =
            Color::fromJson(json["colors"]["inversePrimary"]);

        colors.shadow = Color::fromJson(json["colors"]["shadow"]);
        colors.scrim = Color::fromJson(json["colors"]["scrim"]);
        colors.surfaceTint = Color::fromJson(json["colors"]["surfaceTint"]);

        Brightness brightness = json["brightness"].asString() == "light"
                                    ? Brightness::light
                                    : Brightness::dark;

        return {colors, brightness};
    }

    DynamicScheme
    ColorScheme::buildDynamicScheme(const Renderer::Color &seedColor,
                                    const Brightness brightness,
                                    const float contrast) {

        BESS_ASSERT(contrast >= -1.0f && contrast <= 1.0f,
                    "Contrast must be between -1.0 and 1.0");

        const bool isDark = brightness == Brightness::dark;

        const material_color_utilities::Hct sourceCol(seedColor.toARGB8());

        // SchemeNeutral felt good to me, its not too much in your face
        return material_color_utilities::SchemeNeutral(
            sourceCol, isDark, contrast);
    }
} // namespace Bess::Core::Style
