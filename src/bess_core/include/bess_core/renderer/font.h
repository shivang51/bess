#pragma once

#include "bess_core/renderer/glyph_extractor.h"
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace Bess::Core::Renderer {

    class FontFile {
      public:
        FontFile() = default;
        explicit FontFile(const std::string &path);

        FontFile(FontFile &&other) noexcept;
        FontFile &operator=(FontFile &&other) noexcept;

        FontFile(const FontFile &) = delete;
        FontFile &operator=(const FontFile &) = delete;

        [[nodiscard]] bool isValid() const noexcept;

        bool
        init(float fontSize, char32_t glyphMin = 0, char32_t glyphMax = 128);
        void clearCache();

        [[nodiscard]] Glyph &getGlyph(char32_t ch);
        [[nodiscard]] Glyph &getGlyph(const char *data);

        [[nodiscard]] float getSize() const noexcept;
        [[nodiscard]] char32_t glyphMin() const noexcept;
        [[nodiscard]] char32_t glyphMax() const noexcept;
        [[nodiscard]] std::size_t glyphCount() const noexcept;
        [[nodiscard]] std::size_t cachedGlyphCount() const noexcept;

        [[nodiscard]] float ascent() const;
        [[nodiscard]] float descent() const;
        [[nodiscard]] float lineHeight() const;

      private:
        [[nodiscard]] bool hasTableEntry(char32_t ch) const noexcept;
        Glyph &glyphSlot(char32_t ch);
        bool loadGlyphInto(char32_t ch, Glyph &glyph);

        std::vector<Glyph> m_glyphsTable;
        std::unordered_map<char32_t, Glyph> m_glyphsMap;
        Glyph m_missingGlyph;
        float m_size = 0.f;
        char32_t m_min = 0;
        char32_t m_max = 0;
        GlyphExtractor m_glyphExtractor;
        std::size_t m_glyphCount = 0;
        std::size_t m_cachedGlyphCount = 0;
    };

} // namespace Bess::Core::Renderer
