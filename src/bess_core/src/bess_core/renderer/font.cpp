#include "bess_core/renderer/font.h"
#include "common/logger.h"
#include <utility>

namespace Bess::Core::Renderer {
    namespace {
        constexpr std::size_t kMaxDenseGlyphTableSize = 1 << 20;

        PathProps makeGlyphPathProps() {
            PathProps props{};
            props.renderFill = true;
            props.fillRule = PathFillRule::EvenOdd;
            props.strokeSize = 0.f;
            props.strokeColor.a = 0.f;
            props.closePath = false;
            return props;
        }

        Glyph makeEmptyGlyph(char32_t codepoint) {
            Glyph glyph{};
            glyph.charCode = codepoint;
            glyph.pathProps = makeGlyphPathProps();
            glyph.loaded = true;
            return glyph;
        }
    } // namespace

    FontFile::FontFile(const std::string &path)
        : m_glyphExtractor(path),
          m_glyphCount(m_glyphExtractor.glyphCount()) {}

    FontFile::FontFile(FontFile &&other) noexcept
        : m_glyphsTable(std::move(other.m_glyphsTable)),
          m_glyphsMap(std::move(other.m_glyphsMap)),
          m_missingGlyph(std::move(other.m_missingGlyph)),
          m_size(other.m_size),
          m_min(other.m_min),
          m_max(other.m_max),
          m_glyphExtractor(std::move(other.m_glyphExtractor)),
          m_glyphCount(other.m_glyphCount),
          m_cachedGlyphCount(other.m_cachedGlyphCount) {
        other.m_size = 0.f;
        other.m_min = 0;
        other.m_max = 0;
        other.m_glyphCount = 0;
        other.m_cachedGlyphCount = 0;
    }

    FontFile &FontFile::operator=(FontFile &&other) noexcept {
        if (this == &other) {
            return *this;
        }

        m_glyphsTable = std::move(other.m_glyphsTable);
        m_glyphsMap = std::move(other.m_glyphsMap);
        m_missingGlyph = std::move(other.m_missingGlyph);
        m_size = other.m_size;
        m_min = other.m_min;
        m_max = other.m_max;
        m_glyphExtractor = std::move(other.m_glyphExtractor);
        m_glyphCount = other.m_glyphCount;
        m_cachedGlyphCount = other.m_cachedGlyphCount;

        other.m_size = 0.f;
        other.m_min = 0;
        other.m_max = 0;
        other.m_glyphCount = 0;
        other.m_cachedGlyphCount = 0;
        return *this;
    }

    bool FontFile::isValid() const noexcept {
        return m_glyphExtractor.isValid();
    }

    bool FontFile::init(float fontSize, char32_t glyphMin, char32_t glyphMax) {
        if (!m_glyphExtractor.isValid() || fontSize <= 0.f ||
            glyphMax < glyphMin) {
            clearCache();
            m_size = 0.f;
            return false;
        }

        if (!m_glyphExtractor.setPixelSize(static_cast<int>(fontSize))) {
            clearCache();
            m_size = 0.f;
            return false;
        }

        m_size = fontSize;
        m_min = glyphMin;
        m_max = glyphMax;
        clearCache();

        const auto rangeSize = static_cast<uint64_t>(glyphMax) -
                               static_cast<uint64_t>(glyphMin) + 1;
        if (rangeSize <= kMaxDenseGlyphTableSize) {
            m_glyphsTable.resize(static_cast<std::size_t>(rangeSize));
        } else {
            BESS_WARN("[FontFile] Glyph range is too large for a dense cache "
                      "table; using sparse cache only");
        }
        m_glyphsMap.reserve(64);
        m_missingGlyph = makeEmptyGlyph(0);

        BESS_DEBUG("[FontFile] Reserved lookup table for {} glyphs",
                   m_glyphsTable.size());
        return true;
    }

    void FontFile::clearCache() {
        m_glyphsTable.clear();
        m_glyphsMap.clear();
        m_missingGlyph = makeEmptyGlyph(0);
        m_cachedGlyphCount = 0;
    }

    Glyph &FontFile::getGlyph(char32_t ch) {
        Glyph &slot = glyphSlot(ch);
        if (!slot.loaded || slot.charCode != ch) {
            loadGlyphInto(ch, slot);
        }
        return slot;
    }

    Glyph &FontFile::getGlyph(const char *data) {
        int bytesRead = 0;
        const char32_t ch = static_cast<char32_t>(
            GlyphExtractor::decodeSingleUTF8(data, bytesRead));
        return getGlyph(ch);
    }

    float FontFile::getSize() const noexcept { return m_size; }

    char32_t FontFile::glyphMin() const noexcept { return m_min; }

    char32_t FontFile::glyphMax() const noexcept { return m_max; }

    std::size_t FontFile::glyphCount() const noexcept { return m_glyphCount; }

    std::size_t FontFile::cachedGlyphCount() const noexcept {
        return m_cachedGlyphCount;
    }

    float FontFile::ascent() const { return m_glyphExtractor.ascent(); }

    float FontFile::descent() const { return m_glyphExtractor.descent(); }

    float FontFile::lineHeight() const { return m_glyphExtractor.lineHeight(); }

    bool FontFile::hasTableEntry(char32_t ch) const noexcept {
        return !m_glyphsTable.empty() && ch >= m_min && ch <= m_max;
    }

    Glyph &FontFile::glyphSlot(char32_t ch) {
        if (hasTableEntry(ch)) {
            return m_glyphsTable[static_cast<std::size_t>(ch - m_min)];
        }

        auto [it, _] = m_glyphsMap.try_emplace(ch);
        return it->second;
    }

    bool FontFile::loadGlyphInto(char32_t ch, Glyph &glyph) {
        const bool wasLoaded = glyph.loaded;
        if (m_glyphExtractor.extractGlyph(ch, glyph)) {
            if (!wasLoaded) {
                ++m_cachedGlyphCount;
            }
            return true;
        }

        glyph = makeEmptyGlyph(ch);
        if (!wasLoaded) {
            ++m_cachedGlyphCount;
        }

        BESS_WARN("[FontFile] Failed to load glyph for codepoint {}",
                  static_cast<uint32_t>(ch));
        return false;
    }

} // namespace Bess::Core::Renderer
