#include "scene/renderer/glyph_extractor.h"
#include "common/logger.h"
#include "freetype/freetype.h"
#include <cassert>

namespace Bess::Renderer::Font {
    struct GlyphExtractor::OutlineCollector {
        Path *out = new Path();
        float yFlip;

        glm::vec2 toPx(const FT_Vector &v) const {
            return {float(v.x) / 64.0f, yFlip * (float(v.y) / 64.0f)};
        }

        static int MoveCb(const FT_Vector *to, void *user) {
            auto *self = static_cast<OutlineCollector *>(user);
            Path::PathCommand cmd;
            cmd.kind = Path::PathCommand::Kind::Move;
            cmd.move.p = self->toPx(*to);
            self->out->addCommand(cmd);
            return 0;
        }

        static int LineCb(const FT_Vector *to, void *user) {
            auto *self = static_cast<OutlineCollector *>(user);
            Path::PathCommand cmd;
            cmd.kind = Path::PathCommand::Kind::Line;
            cmd.line.p = self->toPx(*to);
            self->out->addCommand(cmd);
            return 0;
        }

        static int ConicCb(const FT_Vector *control, const FT_Vector *to, void *user) {
            auto *self = static_cast<OutlineCollector *>(user);
            Path::PathCommand cmd;
            cmd.kind = Path::PathCommand::Kind::Quad;
            cmd.quad.c = self->toPx(*control);
            cmd.quad.p = self->toPx(*to);
            self->out->addCommand(cmd);
            return 0;
        }

        static int CubicCb(const FT_Vector *c1, const FT_Vector *c2, const FT_Vector *to, void *user) {
            auto *self = static_cast<OutlineCollector *>(user);
            Path::PathCommand cmd;
            cmd.kind = Path::PathCommand::Kind::Cubic;
            cmd.cubic.c1 = self->toPx(*c1);
            cmd.cubic.c2 = self->toPx(*c2);
            cmd.cubic.p = self->toPx(*to);
            self->out->addCommand(cmd);
            return 0;
        }
    };

    static constexpr FT_Outline_Funcs gOutlineFuncs{
        &GlyphExtractor::OutlineCollector::MoveCb,
        &GlyphExtractor::OutlineCollector::LineCb,
        &GlyphExtractor::OutlineCollector::ConicCb,
        &GlyphExtractor::OutlineCollector::CubicCb,
        0, 0};

    int GlyphExtractor::s_instCount = 0;
    FT_Library GlyphExtractor::s_ftLibrary = nullptr;

    GlyphExtractor::GlyphExtractor(GlyphExtractor &&other) noexcept
        : m_face(other.m_face) {
        s_instCount++;
        other.m_face = nullptr;
    }

    GlyphExtractor &GlyphExtractor::operator=(GlyphExtractor &&other) noexcept {
        s_instCount++;
        if (this != &other) {
            m_face = nullptr;
            m_face = other.m_face;
            other.m_face = nullptr;
        }
        return *this;
    }

    GlyphExtractor::GlyphExtractor(const std::string &fontPath) {
        if (!s_ftLibrary && FT_Init_FreeType(&s_ftLibrary))
            throw std::runtime_error("Failed to init FreeType");

        if (FT_New_Face(s_ftLibrary, fontPath.c_str(), 0, &m_face)) {
            BESS_ERROR("[GlyphExtractor] Failed to load font: {}", fontPath);
            assert(false);
            return;
        }

        bool setCharmapOk = false;
        if (FT_Select_Charmap(m_face, FT_ENCODING_UNICODE) == 0) {
            setCharmapOk = true;
        } else {
            for (int i = 0; i < m_face->num_charmaps; ++i) {
                FT_CharMap cmap = m_face->charmaps[i];
                if (cmap->platform_id == 3 && cmap->encoding_id == 0) { // Symbol
                    FT_Set_Charmap(m_face, cmap);
                    setCharmapOk = true;
                    break;
                }
            }
        }

        if (setCharmapOk) {
            BESS_INFO("[GlyphExtractor] Found unicodes in font file {}", fontPath);
        }

        BESS_INFO("[GlyphExtractor] Found {} glyphs", getGlyphsCount());

        s_instCount++;
    }

    GlyphExtractor::~GlyphExtractor() {
        s_instCount--;
        if (m_face)
            FT_Done_Face(m_face);
        if (s_instCount == 0 && s_ftLibrary) {
            BESS_WARN("Freetype is destroyed");
            FT_Done_FreeType(s_ftLibrary);
            s_ftLibrary = nullptr;
        }
    }

    bool GlyphExtractor::setPixelSize(int pixelHeight) {
        if (!m_face)
            return false;
        return FT_Set_Pixel_Sizes(m_face, 0, pixelHeight) == 0;
    }

    bool GlyphExtractor::extractGlyph(const char *codepoint, Glyph &out, bool yDown) {
        int bytesConsumed = 0;
        return extractGlyph(decodeSingleUTF8(codepoint, bytesConsumed), out, yDown);
    }

    bool GlyphExtractor::extractGlyph(char32_t codepoint, Glyph &out, bool yDown) {
        if (!m_face)
            return false;

        unsigned glyphIndex = FT_Get_Char_Index(m_face, codepoint);
        if (!glyphIndex) {
            BESS_WARN("[GlyphExtractor] Glyph index was not found for {}", (size_t)codepoint);
            return false;
        }

        constexpr FT_Int32 flags = FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING;
        if (FT_Load_Glyph(m_face, glyphIndex, flags))
            return false;
        if (m_face->glyph->format != FT_GLYPH_FORMAT_OUTLINE)
            return false;

        OutlineCollector oc;
        oc.out = &out.path;
        oc.yFlip = yDown ? -1.0f : 1.0f;

        if (FT_Outline_Decompose(&m_face->glyph->outline, &gOutlineFuncs, &oc))
            return false;

        out.advanceX = float(m_face->glyph->advance.x) / 64.0f;
        out.advanceY = float(m_face->glyph->advance.y) / 64.0f;
        out.path = *oc.out;
        out.charCode = codepoint;
        out.path.getPropsRef().renderFill = true;
        out.path.getPropsRef().renderStroke = false;
        out.path.getPropsRef().isClosed = false;
        return true;
    }

    float GlyphExtractor::ascent() const {
        return m_face ? float(m_face->size->metrics.ascender) / 64.0f : 0.0f;
    }

    float GlyphExtractor::descent() const {
        return m_face ? float(m_face->size->metrics.descender) / 64.0f : 0.0f;
    }

    float GlyphExtractor::lineHeight() const {
        return m_face ? float(m_face->size->metrics.height) / 64.0f : 0.0f;
    }

    size_t GlyphExtractor::getGlyphsCount() {
        size_t count = 0;
        FT_ULong charcode = 0;
        FT_UInt gindex = 0;

        charcode = FT_Get_First_Char(m_face, &gindex);
        while (gindex != 0) {
            count++;
            charcode = FT_Get_Next_Char(m_face, charcode, &gindex);
        }

        return count;
    }

    uint32_t GlyphExtractor::decodeSingleUTF8(const char *ptr, int &out_bytes_consumed) {
        unsigned char c = static_cast<unsigned char>(*ptr);

        // 1-byte (0xxxxxxx) - Standard ASCII
        if (c <= 0x7F) {
            out_bytes_consumed = 1;
            return static_cast<uint32_t>(c);
        }
        // 2-bytes
        else if ((c & 0xE0) == 0xC0) {
            out_bytes_consumed = 2;
            return ((static_cast<uint32_t>(c) & 0x1F) << 6) |
                   (static_cast<uint32_t>(ptr[1]) & 0x3F);
        }
        // 3-bytes
        else if ((c & 0xF0) == 0xE0) {
            out_bytes_consumed = 3;
            return ((static_cast<uint32_t>(c) & 0x0F) << 12) |
                   ((static_cast<uint32_t>(ptr[1]) & 0x3F) << 6) |
                   (static_cast<uint32_t>(ptr[2]) & 0x3F);
        }
        // 4-bytes
        else if ((c & 0xF8) == 0xF0) {
            out_bytes_consumed = 4;
            return ((static_cast<uint32_t>(c) & 0x07) << 18) |
                   ((static_cast<uint32_t>(ptr[1]) & 0x3F) << 12) |
                   ((static_cast<uint32_t>(ptr[2]) & 0x3F) << 6) |
                   (static_cast<uint32_t>(ptr[3]) & 0x3F);
        }

        out_bytes_consumed = 1; // Fallback for invalid sequences
        return 0;
    }
} // namespace Bess::Renderer::Font
