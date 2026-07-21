#include "bess_wgpu/text/text_metrics.h"

#include "bess_core/renderer/font.h"
#include "bess_core/ui/icons/cod_icons.h"
#include "bess_core/ui/icons/component_icons.h"
#include "bess_core/ui/icons/font_awesome_icons.h"
#include "bess_core/ui/icons/material_icons.h"
#include "bess_wgpu/text/bitmap_text_pipeline.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace {
    using Bess::Core::Renderer::FontProps;
    using Bess::Wgpu::Text::centeredBaselineOffsetY;
    using Bess::Wgpu::Text::centeredInkBaselineOffsetY;
    using Bess::Wgpu::Text::centeredInkOriginOffsetX;
    using Bess::Wgpu::Text::textLineCount;
    using Bess::Wgpu::Text::TextLineMetrics;

    TEST(BitmapTextFontStackTests, BundledFallbacksCoverEveryRemappedCatalog) {
        struct FallbackGlyph {
            std::string_view fontPath;
            const char *utf8;
        };
        const std::array cases{
            FallbackGlyph{
                .fontPath =
                    Bess::Wgpu::Text::kBundledBitmapFallbackFontPaths[0],
                .utf8 = Bess::UI::Icons::FontAwesomeIcons::FA_CHEVRON_RIGHT,
            },
            FallbackGlyph{
                .fontPath =
                    Bess::Wgpu::Text::kBundledBitmapFallbackFontPaths[1],
                .utf8 = Bess::UI::Icons::CodIcons::CHEVRON_RIGHT,
            },
            FallbackGlyph{
                .fontPath =
                    Bess::Wgpu::Text::kBundledBitmapFallbackFontPaths[2],
                .utf8 = Bess::UI::Icons::ComponentIcons::AND_GATE,
            },
            FallbackGlyph{
                .fontPath =
                    Bess::Wgpu::Text::kBundledBitmapFallbackFontPaths[3],
                .utf8 = Bess::UI::Icons::MaterialIcons::CHEVRON_RIGHT,
            },
        };

        for (const auto &entry : cases) {
            int bytesConsumed = 0;
            const uint32_t codepoint =
                Bess::Core::Renderer::GlyphExtractor::decodeSingleUTF8(
                    entry.utf8, bytesConsumed);
            ASSERT_GT(bytesConsumed, 0) << entry.fontPath;

            Bess::Core::Renderer::FontFile font{std::string{entry.fontPath}};
            ASSERT_TRUE(font.isValid()) << entry.fontPath;
            const auto character = static_cast<char32_t>(codepoint);
            ASSERT_TRUE(font.init(11.f, character, character))
                << entry.fontPath;

            const auto &glyph = font.getGlyph(character);
            EXPECT_EQ(glyph.charCode, character) << entry.fontPath;
            EXPECT_GT(glyph.advanceX, 0.f) << entry.fontPath;
            EXPECT_GT(glyph.width, 0.f) << entry.fontPath;
            EXPECT_GT(glyph.height, 0.f) << entry.fontPath;
        }
    }

    TEST(TextMetricsTests, DescendersNeverShiftASingleLineBaseline) {
        const FontProps props{.fontSize = 12.f};
        const TextLineMetrics metrics{
            .ascender = 10.f,
            .descender = -3.f,
            .lineHeight = 14.f,
        };

        const float reference = centeredBaselineOffsetY("File", props, metrics);
        EXPECT_FLOAT_EQ(centeredBaselineOffsetY("Help", props, metrics),
                        reference);
        EXPECT_FLOAT_EQ(centeredBaselineOffsetY("gypq", props, metrics),
                        reference);
        EXPECT_FLOAT_EQ(centeredBaselineOffsetY("CAPS", props, metrics),
                        reference);
        EXPECT_FLOAT_EQ(reference, 3.5f);
    }

    TEST(TextMetricsTests, IconInkCenterUsesItsVisualBounds) {
        // Font Awesome's 10 px xmark has a 9 px raster spanning [-1, 8]
        // even though its hinted advance is only 8 px.
        EXPECT_FLOAT_EQ(centeredInkOriginOffsetX(9.f, -1.f, 8.f), 1.f);
        EXPECT_FLOAT_EQ(centeredInkOriginOffsetX(8.f, 0.f, 8.f), 0.f);
        EXPECT_FLOAT_EQ(centeredInkOriginOffsetX(9.f, 2.f, -2.f), 0.f);

        EXPECT_FLOAT_EQ(centeredInkBaselineOffsetY(-8.f, 0.f), 4.f);
        EXPECT_FLOAT_EQ(centeredInkBaselineOffsetY(-7.f, 1.f), 3.f);
        EXPECT_FLOAT_EQ(centeredInkBaselineOffsetY(
                            std::numeric_limits<float>::quiet_NaN(), 1.f),
                        0.f);
        EXPECT_FLOAT_EQ(centeredInkBaselineOffsetY(2.f, -2.f), 0.f);
    }

    TEST(TextMetricsTests, CountsMixedLineEndingsAndCentersTheLineBox) {
        const FontProps props{.fontSize = 12.f};
        const TextLineMetrics metrics{
            .ascender = 10.f,
            .descender = -3.f,
            .lineHeight = 14.f,
        };
        constexpr std::string_view text = "one\r\ntwo\nthree\rfour";

        EXPECT_EQ(textLineCount(text), 4u);
        EXPECT_FLOAT_EQ(centeredBaselineOffsetY(text, props, metrics), -17.5f);
    }
} // namespace
