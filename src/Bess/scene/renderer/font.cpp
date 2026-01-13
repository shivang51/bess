#include "scene/renderer/font.h"
#include "common/log.h"

namespace Bess::Renderer::Font {
    FontFile::FontFile(const std::string &path)
        : m_glyphExtractor(path), m_glyphCount(m_glyphExtractor.getGlyphsCount()) {
    }

    FontFile &FontFile::operator=(FontFile &&other) noexcept {
        if (this != &other) {
            m_glyphsTable = std::move(other.m_glyphsTable);
            m_size = other.m_size;
            m_min = other.m_min;
            m_max = other.m_max;
            m_glyphExtractor = std::move(other.m_glyphExtractor);
            m_glyphCount = other.m_glyphCount;

            other.m_size = 0.0f;
            other.m_min = 0;
            other.m_max = 0;
            other.m_glyphCount = 0;
        }
        return *this;
    }

    FontFile::FontFile(FontFile &&other) noexcept
        : m_glyphsTable(std::move(other.m_glyphsTable)),
          m_size(other.m_size),
          m_min(other.m_min),
          m_max(other.m_max),
          m_glyphExtractor(std::move(other.m_glyphExtractor)),
          m_glyphCount(other.m_glyphCount) {
        other.m_size = 0.0f;
        other.m_min = 0;
        other.m_max = 0;
        other.m_glyphCount = 0;
    }

    Glyph &FontFile::getGlyph(char32_t ch) {
        return indexChar(ch);
    }

    Glyph &FontFile::getGlyph(const char *data) {
        auto ch = GlyphExtractor::decodeSingleUTF8(data);
        return indexChar(ch);
    }

    Glyph &FontFile::indexChar(char32_t ch) {
        auto idx = (size_t)ch - m_min;

        if (m_glyphsTable[idx].charCode == ch)
            return m_glyphsTable[idx];

        if (!m_glyphExtractor.extractGlyph(ch, m_glyphsTable[idx])) {
            BESS_WARN("[FontFile] Failed to find glyph for char {}", (char)ch);
        }

        return m_glyphsTable[idx];
    }

    void FontFile::init(float fontSize, size_t glyphMin, size_t glyphMax) {
        m_size = fontSize;
        m_min = glyphMin;
        m_max = glyphMax;

        m_glyphsTable.resize(glyphMax - glyphMin + 1);
        m_glyphExtractor.setPixelSize((int)fontSize);

        BESS_INFO("[FontFile] Reserved lookup table for {} glyphs", m_glyphsTable.size());
    }

    float FontFile::getSize() const {
        return m_size;
    }
} // namespace Bess::Renderer::Font
