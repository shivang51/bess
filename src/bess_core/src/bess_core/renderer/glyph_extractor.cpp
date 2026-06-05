#include "bess_core/renderer/glyph_extractor.h"
#include "common/logger.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#include <algorithm>

namespace Bess::Core::Renderer {
    namespace {
        constexpr uint32_t kReplacementCodepoint = 0xFFFD;

        bool isContinuationByte(const char c) noexcept {
            const auto byte = static_cast<unsigned char>(c);
            return (byte & 0xC0) == 0x80;
        }

        PathProps makeGlyphPathProps() {
            PathProps props{};
            props.renderFill = true;
            props.fillRule = PathFillRule::EvenOdd;
            props.strokeSize = 0.f;
            props.strokeColor.a = 0.f;
            props.closePath = false;
            return props;
        }
    } // namespace

    struct GlyphExtractor::OutlineCollector {
        Path2D *out = nullptr;
        float yFlip = -1.f;
        bool contourOpen = false;

        [[nodiscard]] glm::vec2 toPx(const FT_Vector &v) const noexcept {
            return {static_cast<float>(v.x) / 64.f,
                    yFlip * (static_cast<float>(v.y) / 64.f)};
        }

        void ensureContour(const glm::vec2 &pos) {
            if (!contourOpen) {
                out->moveTo(pos);
                contourOpen = true;
            }
        }

        static int moveCb(const FT_Vector *to, void *user) {
            auto *self = static_cast<OutlineCollector *>(user);
            if (self->contourOpen) {
                self->out->closePath();
            }
            self->out->moveTo(self->toPx(*to));
            self->contourOpen = true;
            return 0;
        }

        static int lineCb(const FT_Vector *to, void *user) {
            auto *self = static_cast<OutlineCollector *>(user);
            const glm::vec2 pos = self->toPx(*to);
            self->ensureContour(pos);
            self->out->lineTo(pos);
            return 0;
        }

        static int conicCb(const FT_Vector *control, const FT_Vector *to,
                           void *user) {
            auto *self = static_cast<OutlineCollector *>(user);
            const glm::vec2 controlPos = self->toPx(*control);
            const glm::vec2 pos = self->toPx(*to);
            self->ensureContour(pos);
            self->out->quadTo(controlPos, pos);
            return 0;
        }

        static int cubicCb(const FT_Vector *c1, const FT_Vector *c2,
                           const FT_Vector *to, void *user) {
            auto *self = static_cast<OutlineCollector *>(user);
            const glm::vec2 control1 = self->toPx(*c1);
            const glm::vec2 control2 = self->toPx(*c2);
            const glm::vec2 pos = self->toPx(*to);
            self->ensureContour(pos);
            self->out->cubicTo(control1, control2, pos);
            return 0;
        }
    };

    static constexpr FT_Outline_Funcs gOutlineFuncs{
        &GlyphExtractor::OutlineCollector::moveCb,
        &GlyphExtractor::OutlineCollector::lineCb,
        &GlyphExtractor::OutlineCollector::conicCb,
        &GlyphExtractor::OutlineCollector::cubicCb,
        0,
        0};

    void *GlyphExtractor::s_ftLibrary = nullptr;
    int GlyphExtractor::s_refCount = 0;

    bool GlyphExtractor::acquireLibrary() {
        if (s_ftLibrary == nullptr) {
            FT_Library library = nullptr;
            if (FT_Init_FreeType(&library) != 0) {
                BESS_ERROR("[GlyphExtractor] Failed to initialize FreeType");
                return false;
            }
            s_ftLibrary = library;
        }

        ++s_refCount;
        return true;
    }

    void GlyphExtractor::releaseLibrary() noexcept {
        if (s_refCount > 0) {
            --s_refCount;
        }

        if (s_refCount == 0 && s_ftLibrary != nullptr) {
            FT_Done_FreeType(static_cast<FT_Library>(s_ftLibrary));
            s_ftLibrary = nullptr;
        }
    }

    GlyphExtractor::GlyphExtractor(const std::string &fontPath) {
        static_cast<void>(loadFont(fontPath));
    }

    GlyphExtractor::~GlyphExtractor() { unload(); }

    GlyphExtractor::GlyphExtractor(GlyphExtractor &&other) noexcept
        : m_face(other.m_face),
          m_hasLibraryRef(other.m_hasLibraryRef) {
        other.m_face = nullptr;
        other.m_hasLibraryRef = false;
    }

    GlyphExtractor &GlyphExtractor::operator=(GlyphExtractor &&other) noexcept {
        if (this == &other) {
            return *this;
        }

        unload();
        m_face = other.m_face;
        m_hasLibraryRef = other.m_hasLibraryRef;
        other.m_face = nullptr;
        other.m_hasLibraryRef = false;
        return *this;
    }

    bool GlyphExtractor::loadFont(const std::string &fontPath) {
        unload();
        if (!acquireLibrary()) {
            return false;
        }
        m_hasLibraryRef = true;

        FT_Face face = nullptr;
        if (FT_New_Face(static_cast<FT_Library>(s_ftLibrary), fontPath.c_str(),
                        0, &face) != 0) {
            BESS_ERROR("[GlyphExtractor] Failed to load font: {}", fontPath);
            unload();
            return false;
        }
        m_face = face;

        bool hasCharmap = FT_Select_Charmap(face, FT_ENCODING_UNICODE) == 0;
        if (!hasCharmap) {
            for (int i = 0; i < face->num_charmaps; ++i) {
                FT_CharMap charmap = face->charmaps[i];
                if (charmap->platform_id == 3 && charmap->encoding_id == 0) {
                    FT_Set_Charmap(face, charmap);
                    hasCharmap = true;
                    break;
                }
            }
        }

        if (!hasCharmap) {
            BESS_WARN("[GlyphExtractor] Font has no Unicode-compatible "
                      "charmap: {}",
                      fontPath);
        }

        return true;
    }

    void GlyphExtractor::unload() noexcept {
        if (m_face != nullptr) {
            FT_Done_Face(static_cast<FT_Face>(m_face));
            m_face = nullptr;
        }

        if (m_hasLibraryRef) {
            releaseLibrary();
            m_hasLibraryRef = false;
        }
    }

    bool GlyphExtractor::setPixelSize(int pixelHeight) {
        if (m_face == nullptr || pixelHeight <= 0) {
            return false;
        }
        return FT_Set_Pixel_Sizes(static_cast<FT_Face>(m_face), 0,
                                  pixelHeight) == 0;
    }

    bool GlyphExtractor::extractGlyph(const char *codepoint, Glyph &out,
                                      bool yDown) {
        int bytesConsumed = 0;
        return extractGlyph(decodeSingleUTF8(codepoint, bytesConsumed), out,
                            yDown);
    }

    bool GlyphExtractor::extractGlyph(char32_t codepoint, Glyph &out,
                                      bool yDown) {
        out = {};
        out.charCode = codepoint;
        out.pathProps = makeGlyphPathProps();

        if (m_face == nullptr) {
            return false;
        }

        FT_Face face = static_cast<FT_Face>(m_face);
        const FT_UInt glyphIndex =
            FT_Get_Char_Index(face, static_cast<FT_ULong>(codepoint));
        if (glyphIndex == 0) {
            BESS_WARN("[GlyphExtractor] Missing glyph for codepoint {}",
                      static_cast<uint32_t>(codepoint));
            return false;
        }

        constexpr FT_Int32 flags =
            FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING | FT_LOAD_NO_AUTOHINT;
        if (FT_Load_Glyph(face, glyphIndex, flags) != 0) {
            return false;
        }

        out.advanceX = static_cast<float>(face->glyph->advance.x) / 64.f;
        out.advanceY = static_cast<float>(face->glyph->advance.y) / 64.f;
        out.bearingX =
            static_cast<float>(face->glyph->metrics.horiBearingX) / 64.f;
        out.bearingY =
            static_cast<float>(face->glyph->metrics.horiBearingY) / 64.f;
        out.width = static_cast<float>(face->glyph->metrics.width) / 64.f;
        out.height = static_cast<float>(face->glyph->metrics.height) / 64.f;

        if (face->glyph->format != FT_GLYPH_FORMAT_OUTLINE) {
            out.bounds = out.path.bounds();
            out.loaded = true;
            return true;
        }

        FT_Outline &outline = face->glyph->outline;
        if (outline.n_points <= 0) {
            out.bounds = out.path.bounds();
            out.loaded = true;
            return true;
        }

        out.path.reserve(
            static_cast<std::size_t>(outline.n_points) +
            static_cast<std::size_t>(std::max<short>(outline.n_contours, 0)) *
                2);

        OutlineCollector collector{
            .out = &out.path,
            .yFlip = yDown ? -1.f : 1.f,
        };

        if (FT_Outline_Decompose(&outline, &gOutlineFuncs, &collector) != 0) {
            out.path.clear();
            out.bounds = {};
            return false;
        }

        if (collector.contourOpen) {
            out.path.closePath();
        }

        out.bounds = out.path.bounds();
        out.loaded = true;
        return true;
    }

    float GlyphExtractor::ascent() const {
        FT_Face face = static_cast<FT_Face>(m_face);
        return (face != nullptr && face->size != nullptr)
                   ? static_cast<float>(face->size->metrics.ascender) / 64.f
                   : 0.f;
    }

    float GlyphExtractor::descent() const {
        FT_Face face = static_cast<FT_Face>(m_face);
        return (face != nullptr && face->size != nullptr)
                   ? static_cast<float>(face->size->metrics.descender) / 64.f
                   : 0.f;
    }

    float GlyphExtractor::lineHeight() const {
        FT_Face face = static_cast<FT_Face>(m_face);
        return (face != nullptr && face->size != nullptr)
                   ? static_cast<float>(face->size->metrics.height) / 64.f
                   : 0.f;
    }

    std::size_t GlyphExtractor::glyphCount() const {
        if (m_face == nullptr) {
            return 0;
        }

        FT_Face face = static_cast<FT_Face>(m_face);
        std::size_t count = 0;
        FT_UInt glyphIndex = 0;
        FT_ULong charCode = FT_Get_First_Char(face, &glyphIndex);
        while (glyphIndex != 0) {
            ++count;
            charCode = FT_Get_Next_Char(face, charCode, &glyphIndex);
        }
        return count;
    }

    uint32_t GlyphExtractor::decodeSingleUTF8(const char *ptr,
                                              int &outBytesConsumed) {
        outBytesConsumed = 0;
        if (ptr == nullptr || *ptr == '\0') {
            return 0;
        }

        const auto first = static_cast<unsigned char>(ptr[0]);
        if (first <= 0x7F) {
            outBytesConsumed = 1;
            return first;
        }

        uint32_t codepoint = 0;
        int length = 0;
        uint32_t minCodepoint = 0;

        if ((first & 0xE0) == 0xC0) {
            codepoint = first & 0x1F;
            length = 2;
            minCodepoint = 0x80;
        } else if ((first & 0xF0) == 0xE0) {
            codepoint = first & 0x0F;
            length = 3;
            minCodepoint = 0x800;
        } else if ((first & 0xF8) == 0xF0) {
            codepoint = first & 0x07;
            length = 4;
            minCodepoint = 0x10000;
        } else {
            outBytesConsumed = 1;
            return kReplacementCodepoint;
        }

        for (int i = 1; i < length; ++i) {
            if (ptr[i] == '\0' || !isContinuationByte(ptr[i])) {
                outBytesConsumed = 1;
                return kReplacementCodepoint;
            }
            codepoint =
                (codepoint << 6) | (static_cast<uint32_t>(ptr[i]) & 0x3F);
        }

        if (codepoint < minCodepoint || codepoint > 0x10FFFF ||
            (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
            outBytesConsumed = 1;
            return kReplacementCodepoint;
        }

        outBytesConsumed = length;
        return codepoint;
    }

} // namespace Bess::Core::Renderer
