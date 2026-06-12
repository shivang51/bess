#pragma once

#include "bess_core/renderer/renderer_2d.h"
#include "bess_wgpu/piplines/custom_quad_pipeline.h"
#include "bess_wgpu/piplines/primitive_pipeline.h"
#include "bess_wgpu/piplines/shadow_pipeline.h"
#include <algorithm>
#include <cmath>

namespace Bess::Wgpu::Renderer2DDetail {

    inline bool isTransparent(const Core::Renderer::QuadProps &props) {
        using Core::Renderer::QuadRenderPass;

        if (props.renderPass == QuadRenderPass::Opaque) {
            return false;
        }
        if (props.renderPass == QuadRenderPass::Transparent) {
            return true;
        }

        if (props.color.a < 0.999f) {
            return true;
        }
        if (props.texture != 0) {
            return true;
        }
        if (props.borderColor.a < 0.999f) {
            return true;
        }
        return false;
    }

    inline void copyColor(float *dst, const Core::Renderer::Color &src) {
        dst[0] = src.r;
        dst[1] = src.g;
        dst[2] = src.b;
        dst[3] = src.a;
    }

    inline void copyVec4(float *dst, const glm::vec4 &src) {
        dst[0] = src.x;
        dst[1] = src.y;
        dst[2] = src.z;
        dst[3] = src.w;
    }

    constexpr uint32_t kShadowShapeRoundedRect = 0;
    constexpr uint32_t kShadowShapeCircle = 1;
    constexpr uint32_t kShadowShapeLine = 2;
    constexpr uint32_t kShadowFlagApplyCameraTransform = 1u << 0u;
    constexpr float kShadowZOffset = 0.0001f;
    constexpr float kShadowGeometryPadding = 2.f;

    inline bool hasDrawableShadow(
        const Core::Renderer::ShadowProps &shadow) {
        return shadow.enabled && shadow.color.a > 0.f;
    }

    inline float shadowGeometryMargin(
        const Core::Renderer::ShadowProps &shadow) {
        return std::max(0.f, shadow.blur) +
               std::max(0.f, shadow.spread) + kShadowGeometryPadding;
    }

    inline void fillShadowCommon(
        Piplines::ShadowInstance &instance,
        const Core::Renderer::ShadowProps &shadow, const glm::vec2 &position,
        float zIndex, float rotation, const glm::vec2 &drawSize,
        uint32_t shapeType, bool applyCameraTransform) {
        instance.position[0] = position.x + shadow.offset.x;
        instance.position[1] = position.y + shadow.offset.y;
        instance.position[2] = zIndex - kShadowZOffset;
        instance.rotation = rotation;
        instance.size[0] = std::max(drawSize.x, 0.f);
        instance.size[1] = std::max(drawSize.y, 0.f);
        instance.blur = std::max(shadow.blur, 0.f);
        instance.spread = shadow.spread;
        copyColor(instance.color, shadow.color);
        instance.shapeType = shapeType;
        instance.flags =
            applyCameraTransform ? kShadowFlagApplyCameraTransform : 0u;
        instance.padding[0] = 0;
        instance.padding[1] = 0;
    }

    inline void makeQuadShadowInstanceInPlace(
        Piplines::ShadowInstance &instance,
        const Core::Renderer::QuadProps &props,
        Core::Renderer::CustomQuadTransformMode transformMode =
            Core::Renderer::CustomQuadTransformMode::Camera) {
        const glm::vec2 sourceSize{std::max(props.size.x, 0.f),
                                   std::max(props.size.y, 0.f)};
        const float margin = shadowGeometryMargin(props.shadow);
        fillShadowCommon(
            instance, props.shadow, props.position, props.zIndex,
            props.rotation, sourceSize + glm::vec2(margin * 2.f),
            kShadowShapeRoundedRect,
            transformMode == Core::Renderer::CustomQuadTransformMode::Camera);
        copyVec4(instance.radii, props.radius);
        instance.shapeData[0] = sourceSize.x;
        instance.shapeData[1] = sourceSize.y;
        instance.shapeData[2] = 0.f;
        instance.shapeData[3] = 0.f;
    }

    inline void makeCircleShadowInstanceInPlace(
        Piplines::ShadowInstance &instance,
        const Core::Renderer::CircleProps &props) {
        const float radius = std::max(props.radius, 0.f);
        const float margin = shadowGeometryMargin(props.shadow);
        const float drawDiameter = (radius + margin) * 2.f;
        fillShadowCommon(instance, props.shadow, props.position, props.zIndex,
                         0.f, glm::vec2(drawDiameter, drawDiameter),
                         kShadowShapeCircle, true);
        instance.radii[0] = 0.f;
        instance.radii[1] = 0.f;
        instance.radii[2] = 0.f;
        instance.radii[3] = 0.f;
        instance.shapeData[0] = radius;
        instance.shapeData[1] = 0.f;
        instance.shapeData[2] = 0.f;
        instance.shapeData[3] = 0.f;
    }

    inline void makeLineShadowInstanceInPlace(
        Piplines::ShadowInstance &instance,
        const Core::Renderer::LineProps &props) {
        const glm::vec2 diff = props.p1 - props.p0;
        const float length = glm::length(diff);
        const float thickness = std::max(props.thickness, 1.f);
        const float angle = std::atan2(diff.y, diff.x);
        const glm::vec2 position = (props.p0 + props.p1) * 0.5f;
        const float margin = shadowGeometryMargin(props.shadow);
        fillShadowCommon(
            instance, props.shadow, position, props.zIndex, angle,
            glm::vec2(length + (margin * 2.f), thickness + (margin * 2.f)),
            kShadowShapeLine, true);
        instance.radii[0] = 0.f;
        instance.radii[1] = 0.f;
        instance.radii[2] = 0.f;
        instance.radii[3] = 0.f;
        instance.shapeData[0] = length;
        instance.shapeData[1] = thickness;
        instance.shapeData[2] = 0.f;
        instance.shapeData[3] = 0.f;
    }

    inline void makePrimitiveInstanceInPlace(
        Piplines::PrimitiveInstance &instance,
        const Core::Renderer::QuadProps &props) {
        instance.position[0] = props.position.x;
        instance.position[1] = props.position.y;
        instance.position[2] = props.zIndex;
        instance.padding0 = 0.f;
        instance.color[0] = props.color.r;
        instance.color[1] = props.color.g;
        instance.color[2] = props.color.b;
        instance.color[3] = props.color.a;
        instance.texData[0] = props.uvRect.x;
        instance.texData[1] = props.uvRect.y;
        instance.texData[2] = props.uvRect.z;
        instance.texData[3] = props.uvRect.w;
        instance.size[0] = props.size.x;
        instance.size[1] = props.size.y;
        instance.id[0] = props.id.runtimeId;
        instance.id[1] = props.id.info;
        instance.primitiveType = 0;
        instance.isMica = 0;
        instance.texSlotIdx = props.texture == 0 ? 0 : 1;
        instance.angle = props.rotation;
        instance.primitiveData[0] = 0.f;
        instance.primitiveData[1] = 0.f;
        instance.primitiveData[2] = 0.f;
        instance.primitiveData[3] = 0.f;

        instance.borderRadius[0] = props.radius.x;
        instance.borderRadius[1] = props.radius.y;
        instance.borderRadius[2] = props.radius.z;
        instance.borderRadius[3] = props.radius.w;
        instance.borderSize[0] = props.thickness.x;
        instance.borderSize[1] = props.thickness.y;
        instance.borderSize[2] = props.thickness.z;
        instance.borderSize[3] = props.thickness.w;
        instance.borderColor[0] = props.borderColor.r;
        instance.borderColor[1] = props.borderColor.g;
        instance.borderColor[2] = props.borderColor.b;
        instance.borderColor[3] = props.borderColor.a;
    }

    constexpr uint32_t kCustomQuadFlagApplyCameraTransform = 1u << 0u;

    inline void makeCustomQuadInstanceInPlace(CustomQuadInstance &instance,
                                              const CustomQuadProps &props) {
        const auto &quad = props.quad;
        instance.position[0] = quad.position.x;
        instance.position[1] = quad.position.y;
        instance.position[2] = quad.zIndex;
        instance.rotation = quad.rotation;
        instance.size[0] = quad.size.x;
        instance.size[1] = quad.size.y;
        instance.padding0[0] = 0.f;
        instance.padding0[1] = 0.f;
        instance.color[0] = quad.color.r;
        instance.color[1] = quad.color.g;
        instance.color[2] = quad.color.b;
        instance.color[3] = quad.color.a;
        instance.uvRect[0] = quad.uvRect.x;
        instance.uvRect[1] = quad.uvRect.y;
        instance.uvRect[2] = quad.uvRect.z;
        instance.uvRect[3] = quad.uvRect.w;
        copyVec4(instance.data0, props.data[0]);
        copyVec4(instance.data1, props.data[1]);
        copyVec4(instance.data2, props.data[2]);
        copyVec4(instance.data3, props.data[3]);
        instance.id[0] = quad.id.runtimeId;
        instance.id[1] = quad.id.info;
        instance.flags[0] =
            props.transformMode ==
                    Core::Renderer::CustomQuadTransformMode::Camera
                ? kCustomQuadFlagApplyCameraTransform
                : 0u;
        instance.flags[1] = 0;
    }

    inline void makeCircleInstanceInPlace(
        Piplines::PrimitiveInstance &instance,
        const Core::Renderer::CircleProps &props) {
        instance.position[0] = props.position.x;
        instance.position[1] = props.position.y;
        instance.position[2] = props.zIndex;
        instance.padding0 = 0.f;
        instance.color[0] = props.color.r;
        instance.color[1] = props.color.g;
        instance.color[2] = props.color.b;
        instance.color[3] = props.color.a;
        instance.texData[0] = 0.f;
        instance.texData[1] = 0.f;
        instance.texData[2] = 1.f;
        instance.texData[3] = 1.f;
        instance.size[0] = props.radius * 2.f;
        instance.size[1] = props.radius * 2.f;
        instance.id[0] = props.id.runtimeId;
        instance.id[1] = props.id.info;
        instance.primitiveType = 1;
        instance.isMica = 0;
        instance.texSlotIdx = 0;
        instance.angle = 0.f;
        instance.primitiveData[0] = props.radius;
        instance.primitiveData[1] =
            props.thickness > 0.f ? props.radius - props.thickness : 0.f;
        instance.primitiveData[2] = 0.f;
        instance.primitiveData[3] = 0.f;

        instance.borderRadius[0] = 0.f;
        instance.borderRadius[1] = 0.f;
        instance.borderRadius[2] = 0.f;
        instance.borderRadius[3] = 0.f;
        instance.borderSize[0] = 0.f;
        instance.borderSize[1] = 0.f;
        instance.borderSize[2] = 0.f;
        instance.borderSize[3] = 0.f;
        instance.borderColor[0] = 0.f;
        instance.borderColor[1] = 0.f;
        instance.borderColor[2] = 0.f;
        instance.borderColor[3] = 0.f;
    }

    inline void makeLineInstanceInPlace(
        Piplines::PrimitiveInstance &instance,
        const Core::Renderer::LineProps &props) {
        glm::vec2 diff = props.p1 - props.p0;
        float length = glm::length(diff);
        float angle = std::atan2(diff.y, diff.x);
        glm::vec2 pos = (props.p0 + props.p1) * 0.5f;
        constexpr float aaPadding = 2.f;
        const float thickness = std::max(props.thickness, 1.f);

        instance.position[0] = pos.x;
        instance.position[1] = pos.y;
        instance.position[2] = props.zIndex;
        instance.padding0 = 0.f;
        instance.color[0] = props.color.r;
        instance.color[1] = props.color.g;
        instance.color[2] = props.color.b;
        instance.color[3] = props.color.a;
        instance.texData[0] = 0.f;
        instance.texData[1] = 0.f;
        instance.texData[2] = 1.f;
        instance.texData[3] = 1.f;
        instance.size[0] = length + (aaPadding * 2.f);
        instance.size[1] = thickness + (aaPadding * 2.f);
        instance.id[0] = props.id.runtimeId;
        instance.id[1] = props.id.info;
        instance.primitiveType = 2;
        instance.isMica = 0;
        instance.texSlotIdx = 0;
        instance.angle = angle;
        instance.primitiveData[0] = length;
        instance.primitiveData[1] = thickness;
        instance.primitiveData[2] = aaPadding;
        instance.primitiveData[3] = 0.f;

        instance.borderRadius[0] = 0.f;
        instance.borderRadius[1] = 0.f;
        instance.borderRadius[2] = 0.f;
        instance.borderRadius[3] = 0.f;
        instance.borderSize[0] = 0.f;
        instance.borderSize[1] = 0.f;
        instance.borderSize[2] = 0.f;
        instance.borderSize[3] = 0.f;
        instance.borderColor[0] = 0.f;
        instance.borderColor[1] = 0.f;
        instance.borderColor[2] = 0.f;
        instance.borderColor[3] = 0.f;
    }

} // namespace Bess::Wgpu::Renderer2DDetail
