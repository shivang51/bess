#include "scene/renderer/skia_renderer.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkM44.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkRRect.h"
#include "include/core/SkRect.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkTypeface.h"
#include "include/effects/SkRuntimeEffect.h"
#include "include/private/base/SkPoint_impl.h"
#include <include/ports/SkFontMgr_fontconfig.h>
#include <iostream>

namespace Bess::Renderer2D {
    SkCanvas *SkiaRenderer::colorCanvas = nullptr;
    sk_sp<SkTypeface> SkiaRenderer::robotoTypeface = nullptr;

    void SkiaRenderer::init() {
        sk_sp<SkFontMgr> fontManager = SkFontMgr_New_FontConfig(nullptr);
        int familyCount = fontManager->countFamilies();
        auto typeface = SkTypeface::MakeEmpty();

        auto fontFilePath = "assets/fonts/Roboto/Roboto-Regular.ttf";
        robotoTypeface = fontManager->makeFromFile(fontFilePath);
        if (robotoTypeface == nullptr) {
            std::cerr << "failed to load font from file " << fontFilePath << std::endl;
            throw;
        }
        SkString fontName;
        robotoTypeface->getFamilyName(&fontName);
        std::cout << "[+] Loaded " << fontName.c_str() << " font" << std::endl;
    }

    void SkiaRenderer::begin(SkCanvas *canvas) {
        colorCanvas = canvas;
    }

    void SkiaRenderer::end() {
    }

    void SkiaRenderer::quad(const int id, const glm::vec3 &pos, const glm::vec2 &size,
                            const glm::vec4 &color) {
        SkPaint paint;
        paint.setColor(vec4NormalizedToSkColor(color));
        paint.setAntiAlias(true);

        SkRect rect = SkRect::MakeXYWH(pos.x, pos.y, size.x, size.y);

        colorCanvas->drawRect(rect, paint);
    }

    void SkiaRenderer::quad(const int id, const glm::vec3 &pos, const glm::vec2 &size,
                            const glm::vec4 &color,
                            const glm::vec4 &borderRadius) {
        SkPaint paint;
        paint.setColor(vec4NormalizedToSkColor(color));
        paint.setAntiAlias(true);

        SkRect rect = SkRect::MakeXYWH(pos.x, pos.y, size.x, size.y);
        SkRRect rRect;

        SkVector radii[] = {
            {borderRadius.x, borderRadius.x},
            {borderRadius.y, borderRadius.y},
            {borderRadius.z, borderRadius.z},
            {borderRadius.w, borderRadius.w},
        };

        rRect.setRectRadii(rect, radii); // Top-left radius);
        colorCanvas->drawRRect(rRect, paint);
    }

    void SkiaRenderer::quad(const int id, const glm::vec3 &pos, const glm::vec2 &size,
                            const glm::vec4 &color,
                            float borderRadius) {
        auto radius = glm::vec4(borderRadius);
        quad(id, pos, size, color, radius);
    }

    void SkiaRenderer::quad(const int id, const glm::vec3 &pos, const glm::vec2 &size,
                            const glm::vec4 &color, const glm::vec4 &borderRadius,
                            const glm::vec4 &borderColor, const glm::vec4 &borderSize) {

        // border
        SkPaint paint;
        paint.setColor(vec4NormalizedToSkColor(borderColor));
        paint.setAntiAlias(true);
        paint.setStroke(true);

        SkPath path;
        auto offset = borderSize / 2.0f;

        // top border
        paint.setStrokeWidth(borderSize.x);
        auto y = pos.y + offset.x;
        path.moveTo(pos.x + borderRadius.x, y);
        path.lineTo(pos.x + size.x - borderRadius.y, y);

        // join top-right
        auto x = pos.x + size.x - offset.y;
        path.arcTo({x, y},
                   {x, y + borderRadius.y},
                   borderRadius.y - offset.y);

        // right border
        paint.setStrokeWidth(borderSize.y);
        path.lineTo(x, pos.y + size.y - borderRadius.z);

        // join bottom-right
        y = pos.y + size.y - offset.z;
        path.arcTo({x, y},
                   {x - borderRadius.z, y},
                   borderRadius.z - offset.z);

        // bottom border
        x = pos.x + borderRadius.z - offset.w;
        paint.setStrokeWidth(borderSize.z);
        path.lineTo(x, pos.y + size.y - offset.z);

        // join bottom-left
        x = pos.x + offset.w;
        path.arcTo({x, y},
                   {x, y - borderRadius.w},
                   borderRadius.w - offset.w);

        // left border
        paint.setStrokeWidth(borderSize.w);
        path.lineTo(x, pos.y + borderRadius.w);

        // join top-left
        y = pos.y + offset.x;
        path.arcTo({x, y},
                   {x + borderRadius.x, y},
                   borderRadius.x - offset.x);

        colorCanvas->drawPath(path, paint);

        // content
        paint.setStroke(false);
        paint.setColor(vec4NormalizedToSkColor(color));
        glm::vec3 pos_ = glm::vec3(pos.x + borderSize.w,
                                   pos.y + borderSize.x, pos.z);
        glm::vec2 size_ = glm::vec2(size.x - borderSize.y - borderSize.w,
                                    size.y - borderSize.z - borderSize.x);

        quad(id, pos_, size_, color, borderRadius);
    }

    void SkiaRenderer::quad(const int id, const glm::vec3 &pos, const glm::vec2 &size,
                            const glm::vec4 &color, float borderRadius,
                            const glm::vec4 &borderColor, float borderSize) {
        quad(id, pos, size, color, glm::vec4(borderRadius), borderColor, glm::vec4(borderSize));
    }

    void SkiaRenderer::cubicBezier(const int id,
                                   const glm::vec3 &start, const glm::vec3 &end,
                                   const glm::vec2 &controlPoint1,
                                   const glm::vec2 &controlPoint2,
                                   float weight, const glm::vec4 &color) {
        SkPath path;
        path.moveTo(vec2ToSkPoint(glm::vec2(start)));
        path.cubicTo(vec2ToSkPoint(glm::vec2(controlPoint1)),
                     vec2ToSkPoint(glm::vec2(controlPoint2)),
                     vec2ToSkPoint(glm::vec2(end)));

        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setStroke(true);
        paint.setStrokeWidth(weight);
        paint.setColor(vec4NormalizedToSkColor(color));

        colorCanvas->drawPath(path, paint);
    }

    void SkiaRenderer::circle(const int id, const glm::vec3 &center, float radius,
                              const glm::vec4 &color) {
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setColor(vec4NormalizedToSkColor(color));

        colorCanvas->drawCircle(vec2ToSkPoint(glm::vec2(center)),
                                radius, paint);
    }

    void SkiaRenderer::circle(const int id, const glm::vec3 &center, float radius,
                              const glm::vec4 &color,
                              const glm::vec4 &borderColor, float borderSize) {
        circle(id, center, radius, color);
        circle(id, center, radius, borderColor, borderSize);
    }

    void SkiaRenderer::circle(const int id, const glm::vec3 &center, float radius,
                              const glm::vec4 &borderColor, float borderSize) {

        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setStrokeWidth(borderSize);
        paint.setColor(vec4NormalizedToSkColor(borderColor));
        paint.setStroke(true);

        colorCanvas->drawCircle(vec2ToSkPoint(glm::vec2(center)),
                                radius, paint);
    }

    void SkiaRenderer::grid(const int id,
                            const glm::vec3 &pos, const glm::vec2 &cameraOffset,
                            const glm::vec2 &size, float zoom,
                            const glm::vec4 &color, bool dotted) {
        const char *sksl =
            "uniform float zoom;"
            "uniform vec2 cameraOffset;"
            "uniform float gap;"
            "uniform float dotSize;"
            "uniform vec4 color; "
            "half4 main(float2 fragCoord) {"
            "   vec2 gridPos = mod((fragCoord / zoom) - cameraOffset, gap);"
            "   float dist = distance(gridPos, vec2(gap / 2.0));"
            "   float alpha = smoothstep(dotSize, dotSize - 1.0, dist);"
            "   if (alpha == 0.0) {"
            "       return half4(0.0, 0.0, 0.0, 0.0);"
            "   }"
            "   return color * alpha;"
            "}";

        // Parse the SkSL, and create an SkRuntimeEffect object:
        auto [effect, err] = SkRuntimeEffect::MakeForShader(SkString(sksl));

        if (err.size() > 0) {
            std::cout << err.c_str() << std::endl;
            throw;
        }

        SkRuntimeShaderBuilder builder(effect);
        builder.uniform("zoom") = zoom;
        builder.uniform("cameraOffset") = SkV2{cameraOffset.x, cameraOffset.y};
        builder.uniform("gap") = 20.0f;
        builder.uniform("dotSize") = 1.5f;
        builder.uniform("color") = vec4ColorToSkV4(color);

        // Create an SkShader from our SkSL, with `imageShader` bound to `image`:
        sk_sp<SkShader> myShader = builder.makeShader();

        SkRect rect;
        rect.setXYWH(pos.x, pos.y, size.x, size.y);

        SkPaint paint;
        paint.setShader(myShader);

        colorCanvas->drawRect(rect, paint);
    }

    void SkiaRenderer::text(const int id, const std::string &data,
                            const glm::vec3 &pos, const size_t size,
                            const glm::vec4 &color) {
        SkPaint paint;
        paint.setColor(vec4NormalizedToSkColor(color));

        SkFont font;
        font.setSize(size);
        font.setTypeface(robotoTypeface);

        colorCanvas->drawSimpleText(data.c_str(), data.size(), SkTextEncoding::kUTF8,
                                    0, 0,
                                    font, paint);
    }

    void SkiaRenderer::line(const int id, const glm::vec3 &start, const glm::vec3 &end,
                            float weight, const glm::vec4 &color) {
        line(id, {start, end}, weight, color);
    }

    void SkiaRenderer::line(const int id, const std::vector<glm::vec3> &vertices,
                            float weight, const glm::vec4 &color) {
        SkPaint paint;
        paint.setColor(vec4NormalizedToSkColor(color));
        paint.setStrokeWidth(weight);
        paint.setStroke(true);
        paint.setAntiAlias(true);
        paint.setStrokeJoin(SkPaint::kRound_Join);

        assert(vertices.size() > 1);

        SkPath path;
        path.moveTo(vertices[0].x, vertices[0].y);
        for (int i = 1; i < (int)vertices.size(); i++) {
            auto vertex = vertices[i];
            path.lineTo(vertex.x, vertex.y);
        }

        colorCanvas->drawPath(path, paint);
    }

    SkPoint SkiaRenderer::vec2ToSkPoint(const glm::vec2 &point) {
        return SkPoint(point.x, point.y);
    }

    SkColor SkiaRenderer::vec4ToSkColor(const glm::vec4 &color) {
        return SkColorSetARGB(color.w, color.r, color.g, color.b);
    }

    SkColor SkiaRenderer::vec4NormalizedToSkColor(const glm::vec4 &color) {
        auto color_ = color;
        color_ *= 255.0f;
        return SkColorSetARGB(color_.w, color_.r, color_.g, color_.b);
    }

    SkV4 SkiaRenderer::vec4ColorToSkV4(const glm::vec4 &color) {
        auto color_ = color / 255.0f;
        return SkV4{color_.x, color_.y, color_.z, color_.w};
    }

} // namespace Bess::Renderer2D
