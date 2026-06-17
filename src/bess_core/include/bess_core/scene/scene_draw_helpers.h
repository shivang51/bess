#pragma once

#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/renderer/renderer_types.h"
#include "bess_core/scene/scene_draw_context.h"
#include <algorithm>
#include <cstddef>
#include <string_view>

namespace Bess::Canvas::SceneDraw {
    struct ShadowStyle {
        bool enabled = false;
        bool useInvalidId = true;
        glm::vec2 offset = {5.f, 5.f};
        glm::vec2 scale = {1.f, 1.f};
        float blur = 8.f;
        float spread = 0.f;
        glm::vec4 color = {0.f, 0.f, 0.f, 0.5f};
        Core::Renderer::TextureHandle texture = 0;
    };

    struct QuadStyle {
        float angle = 0.f;
        Core::Renderer::Color borderColor = {0.f, 0.f, 0.f, 0.f};
        glm::vec4 borderRadius = {0.f, 0.f, 0.f, 0.f};
        glm::vec4 borderSize = {0.f, 0.f, 0.f, 0.f};
        ShadowStyle shadow{};
    };

    struct PathStyle {
        bool closePath = false;
        bool renderFill = false;
        glm::vec4 fillColor = glm::vec4(1.f);
        bool renderStroke = true;
        bool roundedJoints = false;
        Core::Renderer::QuadRenderPass renderPass =
            Core::Renderer::QuadRenderPass::Auto;
    };

    [[nodiscard]] inline bool hasAnyNonZero(const glm::vec4 &value) {
        return value.x != 0.f || value.y != 0.f || value.z != 0.f ||
               value.w != 0.f;
    }

    [[nodiscard]] inline glm::vec2 xy(const glm::vec3 &value) {
        return {value.x, value.y};
    }

    [[nodiscard]] inline Core::Renderer::QuadProps
    makeQuadProps(const glm::vec3 &pos,
                  const glm::vec2 &size,
                  const glm::vec4 &color,
                  const PickingId &id,
                  const QuadStyle &style = {}) {
        Core::Renderer::QuadProps props;
        props.position = xy(pos);
        props.size = size;
        props.rotation = style.angle;
        props.zIndex = pos.z;
        props.color = color;
        props.id = id;
        props.borderColor = style.borderColor;
        props.radius = style.borderRadius;
        props.thickness = style.borderSize;
        props.shadow.enabled = style.shadow.enabled;
        props.shadow.offset = style.shadow.offset;
        props.shadow.blur = style.shadow.blur;
        props.shadow.spread = style.shadow.spread;
        props.shadow.color = style.shadow.color;
        return props;
    }

    inline void drawQuad(SceneDrawContext &context,
                         const glm::vec3 &pos,
                         const glm::vec2 &size,
                         const glm::vec4 &color,
                         const PickingId &id,
                         const QuadStyle &style = {}) {
        if (!context.renderer) {
            return;
        }

        auto props = makeQuadProps(pos, size, color, id, style);
        props.transformMode = context.transformMode;
        context.renderer->drawQuad(props);
    }

    inline void drawCircle(SceneDrawContext &context,
                           const glm::vec3 &center,
                           float radius,
                           const glm::vec4 &color,
                           const PickingId &id,
                           float innerRadius = 0.f) {
        if (!context.renderer) {
            return;
        }

        Core::Renderer::CircleProps props;
        props.position = xy(center);
        props.radius = radius;
        props.thickness =
            innerRadius > 0.f ? std::max(0.f, radius - innerRadius) : 0.f;
        props.zIndex = center.z;
        props.color = color;
        props.id = id;
        props.transformMode = context.transformMode;
        context.renderer->drawCircle(props);
    }

    inline void drawLine(SceneDrawContext &context,
                         const glm::vec3 &start,
                         const glm::vec3 &end,
                         float thickness,
                         const glm::vec4 &color,
                         const PickingId &id) {
        if (!context.renderer) {
            return;
        }

        Core::Renderer::LineProps props;
        props.p0 = xy(start);
        props.p1 = xy(end);
        props.thickness = thickness;
        props.zIndex = (start.z + end.z) * 0.5f;
        props.color = color;
        props.id = id;
        props.transformMode = context.transformMode;
        context.renderer->drawLine(props);
    }

    inline void drawText(SceneDrawContext &context,
                         std::string_view text,
                         const glm::vec3 &pos,
                         std::size_t size,
                         const glm::vec4 &color,
                         const PickingId &id,
                         float angle = 0.f) {
        (void)angle;
        if (!context.renderer) {
            return;
        }

        Core::Renderer::FontProps props;
        props.position = xy(pos);
        props.fontSize = static_cast<float>(size);
        props.color = color;
        props.zIndex = pos.z;
        props.id = id;
        props.transformMode = context.transformMode;
        context.renderer->drawFont(text, props);
    }

    [[nodiscard]] inline Core::Renderer::PathProps
    makePathProps(const glm::vec3 &startPos,
                  float strokeSize,
                  const glm::vec4 &strokeColor,
                  const PickingId &id,
                  const PathStyle &style = {}) {
        Core::Renderer::PathProps props;
        props.fillColor = style.fillColor;
        props.strokeColor = style.renderStroke
                                ? Core::Renderer::Color(strokeColor)
                                : Core::Renderer::Color(0.f, 0.f, 0.f, 0.f);
        props.strokeSize = style.renderStroke ? strokeSize : 0.f;
        props.renderFill = style.renderFill;
        props.zIndex = startPos.z;
        props.id = id;
        props.renderPass = style.renderPass;
        props.lineJoin = style.roundedJoints
                             ? Core::Renderer::PathLineJoin::Round
                             : Core::Renderer::PathLineJoin::Miter;
        props.closePath = style.closePath;
        return props;
    }

    inline void beginPath(SceneDrawContext &context,
                          const glm::vec3 &startPos,
                          float strokeSize,
                          const glm::vec4 &strokeColor,
                          const PickingId &id,
                          const PathStyle &style = {}) {
        if (!context.renderer) {
            return;
        }

        auto props =
            makePathProps(startPos, strokeSize, strokeColor, id, style);
        props.transformMode = context.transformMode;
        context.renderer->beginPath(props);
        context.renderer->pathMoveTo(xy(startPos));
    }

    inline void pathLineTo(SceneDrawContext &context,
                           const glm::vec3 &pos,
                           float strokeSize) {
        if (!context.renderer) {
            return;
        }

        context.renderer->pathLineTo(
            xy(pos), Core::Renderer::PathCommandStroke::withWidth(strokeSize));
    }

    inline void pathLineTo(SceneDrawContext &context,
                           const glm::vec3 &pos,
                           float strokeSize,
                           const PickingId &id) {
        if (!context.renderer) {
            return;
        }

        context.renderer->pathLineTo(
            xy(pos),
            Core::Renderer::PathCommandStroke::withWidthAndId(strokeSize, id));
    }

    inline void pathCubicTo(SceneDrawContext &context,
                            const glm::vec3 &end,
                            const glm::vec2 &controlPoint1,
                            const glm::vec2 &controlPoint2,
                            float strokeSize) {
        if (!context.renderer) {
            return;
        }

        context.renderer->pathCubicTo(
            controlPoint1,
            controlPoint2,
            xy(end),
            Core::Renderer::PathCommandStroke::withWidth(strokeSize));
    }

    inline void pathCubicTo(SceneDrawContext &context,
                            const glm::vec3 &end,
                            const glm::vec2 &controlPoint1,
                            const glm::vec2 &controlPoint2,
                            float strokeSize,
                            const PickingId &id) {
        if (!context.renderer) {
            return;
        }

        context.renderer->pathCubicTo(
            controlPoint1,
            controlPoint2,
            xy(end),
            Core::Renderer::PathCommandStroke::withWidthAndId(strokeSize, id));
    }

    inline void endPath(SceneDrawContext &context) {
        if (!context.renderer) {
            return;
        }

        context.renderer->endPath();
    }
} // namespace Bess::Canvas::SceneDraw
