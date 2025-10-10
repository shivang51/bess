#include "scene/renderer/glyph_extractor.h"
#include <iostream>

namespace Bess::Renderer::Font {
    struct GlyphExtractor::OutlineCollector {
        GlyphPath *out;
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
            std::cerr << "Failed to init FreeType\n";
            return;
        }

        if (FT_New_Face(m_ft, fontPath.c_str(), 0, &m_face)) {
            std::cerr << "Failed to load font: " << fontPath << "\n";
            FT_Done_FreeType(m_ft);
            m_ft = nullptr;
            return;
        }
    }

    GlyphExtractor::~GlyphExtractor() {
        if (m_face)
            FT_Done_Face(m_face);
        if (m_ft)
            FT_Done_FreeType(m_ft);
    }

    bool GlyphExtractor::setPixelSize(int pixelHeight, unsigned dpi) {
        if (!m_face)
            return false;
        return FT_Set_Char_Size(m_face, 0, pixelHeight * 64, dpi, dpi) == 0;
    }

    bool GlyphExtractor::extractGlyph(char32_t codepoint, GlyphPath &out, bool yDown) {
        if (!m_face)
            return false;

        unsigned glyphIndex = FT_Get_Char_Index(m_face, codepoint);
        if (!glyphIndex)
            return false;

        FT_Int32 flags = FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING;
        if (FT_Load_Glyph(m_face, glyphIndex, flags))
            return false;
        if (m_face->glyph->format != FT_GLYPH_FORMAT_OUTLINE)
            return false;

        OutlineCollector oc;
        oc.out = &out;
        oc.yFlip = yDown ? -1.0f : 1.0f;

        if (FT_Outline_Decompose(&m_face->glyph->outline, &gOutlineFuncs, &oc))
            return false;

        out.advanceX = float(m_face->glyph->advance.x) / 64.0f;
        out.advanceY = float(m_face->glyph->advance.y) / 64.0f;
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
} // namespace Bess::Renderer::Font
