#pragma once

#include "bess_core/renderer/renderer_path.h"
#include "bess_core/renderer/renderer_types.h"
#include <cstddef>
#include <cstdint>
#include <string>

namespace Bess::Core::Renderer {

    struct Glyph {
        char32_t charCode{};
        Path2D path;
        PathProps pathProps{};
        PathBounds bounds{};
        float advanceX = 0.f;
        float advanceY = 0.f;
        float bearingX = 0.f;
        float bearingY = 0.f;
        float width = 0.f;
        float height = 0.f;
        bool loaded = false;
    };

    class GlyphExtractor {
      public:
        GlyphExtractor() = default;
        explicit GlyphExtractor(const std::string &fontPath);
        ~GlyphExtractor();

        GlyphExtractor(GlyphExtractor &&other) noexcept;
        GlyphExtractor &operator=(GlyphExtractor &&other) noexcept;

        GlyphExtractor(const GlyphExtractor &) = delete;
        GlyphExtractor &operator=(const GlyphExtractor &) = delete;

        [[nodiscard]] bool loadFont(const std::string &fontPath);
        void unload() noexcept;

        [[nodiscard]] static uint32_t decodeSingleUTF8(const char *ptr,
                                                       int &outBytesConsumed);

        [[nodiscard]] std::size_t glyphCount() const;
        [[nodiscard]] std::size_t getGlyphsCount() const {
            return glyphCount();
        }

        [[nodiscard]] bool isValid() const noexcept {
            return m_face != nullptr;
        }

        bool setPixelSize(int pixelHeight);

        bool extractGlyph(char32_t codepoint, Glyph &out, bool yDown = true);
        bool extractGlyph(const char *codepoint, Glyph &out, bool yDown = true);

        [[nodiscard]] float ascent() const;
        [[nodiscard]] float descent() const;
        [[nodiscard]] float lineHeight() const;

        struct OutlineCollector;

      private:
        static bool acquireLibrary();
        static void releaseLibrary() noexcept;

        void *m_face = nullptr;
        bool m_hasLibraryRef = false;

        static void *s_ftLibrary;
        static int s_refCount;
    };

} // namespace Bess::Core::Renderer
