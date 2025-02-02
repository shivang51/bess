#pragma once

#include "glm.hpp"
#include "include/core/SkCanvas.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkTypeface.h"
#include "include/private/base/SkPoint_impl.h"
#include <vector>

namespace Bess::Renderer2D {
    class SkiaRenderer {
      public: // intialization and cleanup
        static void init();
        static void begin(SkCanvas *canvas);
        static void end();

      public: // quads
        static void quad(const int id, const glm::vec3 &pos, const glm::vec2 &size,
                         const glm::vec4 &color);

        static void quad(const int id, const glm::vec3 &pos, const glm::vec2 &size,
                         const glm::vec4 &color,
                         const glm::vec4 &borderRadius);

        static void quad(const int id, const glm::vec3 &pos, const glm::vec2 &size,
                         const glm::vec4 &color,
                         float borderRadius);

        static void quad(const int id, const glm::vec3 &pos, const glm::vec2 &size,
                         const glm::vec4 &color, const glm::vec4 &borderRadius,
                         const glm::vec4 &borderColor, const glm::vec4 &borderSize);

        static void quad(const int id, const glm::vec3 &pos, const glm::vec2 &size,
                         const glm::vec4 &color, float borderRadius,
                         const glm::vec4 &borderColor, const glm::vec4 &borderSize);

        static void quad(const int id, const glm::vec3 &pos, const glm::vec2 &size,
                         const glm::vec4 &color, const glm::vec4 &borderRadius,
                         const glm::vec4 &borderColor, float borderSize);

        static void quad(const int id, const glm::vec3 &pos, const glm::vec2 &size,
                         const glm::vec4 &color, float borderRadius,
                         const glm::vec4 &borderColor, float borderSize);

      public: // curves
        static void cubicBezier(const int id,
                                const glm::vec3 &start, const glm::vec3 &end,
                                const glm::vec2 &controlPoint1, const glm::vec2 &controlPoint2,
                                float weight, const glm::vec4 &color);

      public: // circles
        static void circle(const int id, const glm::vec3 &center, float radius,
                           const glm::vec4 &color);

        static void circle(const int id, const glm::vec3 &center, float radius,
                           const glm::vec4 &color,
                           const glm::vec4 &borderColor, float borderSize);

        // circle without any fill, a hollow circle
        static void circle(const int id, const glm::vec3 &center, float radius,
                           const glm::vec4 &borderColor, float borderSize);

      public: // grid
        static void grid(const int id,
                         const glm::vec3 &pos, const glm::vec2 &cameraOffset,
                         const glm::vec2 &size, float zoom,
                         const glm::vec4 &color, bool dotted = true);

      public: // text
        static void text(const int id, const std::string &data,
                         const glm::vec3 &pos, const size_t size,
                         const glm::vec4 &color);

      public: // lines
        static void line(const int id, const glm::vec3 &start, const glm::vec3 &end,
                         float weight, const glm::vec4 &color);

        static void line(const int id, const std::vector<glm::vec3> &vertices,
                         float weight, const glm::vec4 &color);

      public:
        static SkPoint vec2ToSkPoint(const glm::vec2 &point);
        static SkColor vec4ToSkColor(const glm::vec4 &color);
        static SkColor vec4NormalizedToSkColor(const glm::vec4 &color);
        static SkV4 vec4ColorToSkV4(const glm::vec4 &color);

      private:
        static SkCanvas *colorCanvas;
        static sk_sp<SkTypeface> robotoTypeface;
    };

} // namespace Bess::Renderer2D
