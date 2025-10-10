#include "scene/renderer/glyph_extractor.h"
#include "common/log.h"
#include "freetype/freetype.h"
#include <cassert>
#include <iostream>

namespace Bess::Renderer::Font {
    struct GlyphExtractor::OutlineCollector {
        CharacterPath *out;
        float yFlip;

        Vec2 toPx(const FT_Vector &v) const {
            return {float(v.x) / 64.0f, yFlip * (float(v.y) / 64.0f)};
        }

        static int MoveCb(const FT_Vector *to, void *user) {
            auto *self = static_cast<OutlineCollector *>(user);
            PathCommand cmd;
            cmd.kind = PathCommand::Kind::Move;
            cmd.move.p = self->toPx(*to);
            self->out->cmds.push_back(cmd);
            return 0;
        }

        static int LineCb(const FT_Vector *to, void *user) {
            auto *self = static_cast<OutlineCollector *>(user);
            PathCommand cmd;
            cmd.kind = PathCommand::Kind::Line;
            cmd.line.p = self->toPx(*to);
            self->out->cmds.push_back(cmd);
            return 0;
        }

        static int ConicCb(const FT_Vector *control, const FT_Vector *to, void *user) {
            auto *self = static_cast<OutlineCollector *>(user);
            PathCommand cmd;
            cmd.kind = PathCommand::Kind::Quad;
            cmd.quad.c = self->toPx(*control);
            cmd.quad.p = self->toPx(*to);
            self->out->cmds.push_back(cmd);
            return 0;
        }

        static int CubicCb(const FT_Vector *c1, const FT_Vector *c2, const FT_Vector *to, void *user) {
            auto *self = static_cast<OutlineCollector *>(user);
            PathCommand cmd;
            cmd.kind = PathCommand::Kind::Cubic;
            cmd.cubic.c1 = self->toPx(*c1);
            cmd.cubic.c2 = self->toPx(*c2);
            cmd.cubic.p = self->toPx(*to);
            self->out->cmds.push_back(cmd);
            return 0;
        }
    };

    static constexpr FT_Outline_Funcs gOutlineFuncs{
        &GlyphExtractor::OutlineCollector::MoveCb,
        &GlyphExtractor::OutlineCollector::LineCb,
        &GlyphExtractor::OutlineCollector::ConicCb,
        &GlyphExtractor::OutlineCollector::CubicCb,
        0, 0};

    GlyphExtractor::GlyphExtractor(const std::string &fontPath) {
        if (FT_Init_FreeType(&m_ft)) {
            BESS_ERROR("[GlyphExtractor] Failed to init FreeType");
            return;
        }

        if (FT_New_Face(m_ft, fontPath.c_str(), 0, &m_face)) {
            BESS_ERROR("[GlyphExtractor] Failed to load font: {}", fontPath);
            FT_Done_FreeType(m_ft);
            assert(false);
            m_ft = nullptr;
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
    }

    GlyphExtractor::~GlyphExtractor() {
        if (m_face)
            FT_Done_Face(m_face);
        if (m_ft)
            FT_Done_FreeType(m_ft);
    }

    bool GlyphExtractor::setPixelSize(int pixelHeight) {
        if (!m_face)
            return false;
        return FT_Set_Pixel_Sizes(m_face, 0, pixelHeight) == 0;
    }

    bool GlyphExtractor::extractGlyph(const char *codepoint, Glyph &out, bool yDown) {
        return extractGlyph(decodeSingleUTF8(codepoint), out, yDown);
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

        out.path.advanceX = float(m_face->glyph->advance.x) / 64.0f;
        out.path.advanceY = float(m_face->glyph->advance.y) / 64.0f;
        out.charCode = codepoint;
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

    char32_t GlyphExtractor::decodeSingleUTF8(std::string_view utf8) {
        const unsigned char *s = reinterpret_cast<const unsigned char *>(utf8.data());
        if (utf8.empty())
            return U'\0';

        if (s[0] < 0x80) // 1-byte ASCII
            return s[0];
        else if ((s[0] & 0xE0) == 0xC0) // 2-byte
            return ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        else if ((s[0] & 0xF0) == 0xE0) // 3-byte
            return ((s[0] & 0x0F) << 12) |
                   ((s[1] & 0x3F) << 6) |
                   (s[2] & 0x3F);
        else if ((s[0] & 0xF8) == 0xF0) // 4-byte
            return ((s[0] & 0x07) << 18) |
                   ((s[1] & 0x3F) << 12) |
                   ((s[2] & 0x3F) << 6) |
                   (s[3] & 0x3F);

        return U'\0'; // invalid input
    }
} // namespace Bess::Renderer::Font
