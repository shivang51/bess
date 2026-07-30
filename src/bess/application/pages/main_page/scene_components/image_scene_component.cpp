#include "pages/main_page/scene_components/image_scene_component.h"

#include "pages/main_page/comp_edit.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/camera.h"
#include "bess_core/scene/scene_draw_helpers.h"
#include "bess_core/settings/viewport_theme.h"
#include "bess_wgpu/wgpu_texture.h"
#include "common/logger.h"
#include "gtc/type_ptr.hpp"
#include "imgui.h"
#include "ui/icons/FontAwesomeIcons_Remapped.h"
#include "ui/widgets/m_widgets.h"
#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>

namespace Icons = Bess::UI::Icons;
namespace Widgets = Bess::UI::Widgets;

namespace Bess::Canvas {
    namespace {
        constexpr uint32_t kBodyInfo = 0u;
        constexpr uint32_t kResizeTopLeftInfo = 1u;
        constexpr uint32_t kResizeTopRightInfo = 2u;
        constexpr uint32_t kResizeBottomRightInfo = 3u;
        constexpr uint32_t kResizeBottomLeftInfo = 4u;

        constexpr float kDefaultImageSide = 100.f;
        constexpr float kMaxInitialImageSide = 360.f;
        constexpr float kMinResizeSide = 16.f;
        constexpr float kHandleScreenSize = 8.f;
        constexpr float kHandleBorderScreenSize = 1.5f;

        [[nodiscard]] float safeZoom(const std::shared_ptr<Camera> &camera) {
            return camera ? std::max(0.01f, camera->getZoom()) : 1.f;
        }

        [[nodiscard]] float screenToWorld(float value,
                                          const std::shared_ptr<Camera> &camera) {
            return value / safeZoom(camera);
        }

        [[nodiscard]] glm::vec2 sanitizedScale(glm::vec2 scale) {
            scale.x = std::isfinite(scale.x) ? scale.x : kDefaultImageSide;
            scale.y = std::isfinite(scale.y) ? scale.y : kDefaultImageSide;
            scale.x = std::max(kMinResizeSide, scale.x);
            scale.y = std::max(kMinResizeSide, scale.y);
            return scale;
        }

        [[nodiscard]] glm::vec2 resizeSign(uint32_t info) {
            switch (info) {
            case kResizeTopLeftInfo:
                return {-1.f, -1.f};
            case kResizeTopRightInfo:
                return {1.f, -1.f};
            case kResizeBottomRightInfo:
                return {1.f, 1.f};
            case kResizeBottomLeftInfo:
                return {-1.f, 1.f};
            default:
                return {0.f, 0.f};
            }
        }

        [[nodiscard]] float snappedSize(float value) {
            value = std::max(kMinResizeSide, value);
            value = std::round(value / SNAP_ANOUNT) * SNAP_ANOUNT;
            return std::max(kMinResizeSide, value);
        }
    } // namespace

    ImageSceneComponent::ImageSceneComponent() {
        m_name = "Image";
        m_icon = Icons::FontAwesomeIcons::FA_IMAGE;
        m_style.color = ViewportTheme::colors.componentBG;
    }

    std::vector<std::shared_ptr<SceneComponent>>
    ImageSceneComponent::clone(const SceneState &sceneState) const {
        (void)sceneState;
        auto clonedComponent = std::make_shared<ImageSceneComponent>(*this);
        prepareClone(*clonedComponent);
        return {clonedComponent};
    }

    void ImageSceneComponent::draw(SceneDrawContext &context) {
        if (context.sceneState == nullptr || context.renderer == nullptr) {
            return;
        }

        if (m_isFirstDraw) {
            if (m_transform.scale.x <= 0.f || m_transform.scale.y <= 0.f) {
                setScale(calculateScale(*context.sceneState));
            }
            m_isFirstDraw = false;
        }

        const auto pos =
            getAbsolutePosition(*context.sceneState, context.isSchematicMode);
        const auto bodyId = PickingId{m_runtimeId, kBodyInfo};

        drawImageQuad(context, pos, bodyId);

        if (m_isSelected) {
            drawSelectionControls(context, pos);
        }
    }

    void ImageSceneComponent::drawSchematic(SceneDrawContext &context) {
        draw(context);
    }

    std::type_index ImageSceneComponent::getTypeIndex() {
        return typeid(ImageSceneComponent);
    }

    void ImageSceneComponent::drawPropertiesUI(SceneState &sceneState) {
        NonSimSceneComponent::drawPropertiesUI(sceneState);

        if (Widgets::TreeNode(0, "Image Properties")) {
            ImGui::Text("Source: %u x %u", m_imageWidth, m_imageHeight);
            ImGui::Text("Bytes: %zu", m_imageData.size());
            if (ImGui::Checkbox("Maintain Aspect Ratio",
                                &m_maintainAspectRatio) &&
                m_maintainAspectRatio) {
                setScale(scaleMaintainingAspect(m_transform.scale, true));
            }

            const auto prevSize = sanitizedScale(m_transform.scale);
            glm::vec2 size = m_transform.scale;
            if (ImGui::DragFloat2("Size",
                                  glm::value_ptr(size),
                                  1.f,
                                  kMinResizeSide,
                                  8192.f)) {
                setScale(scaleFromPropertiesEdit(prevSize, size));
            }

            ImGui::TreePop();
        }
    }

    void ImageSceneComponent::onMouseDragged(
        const Events::MouseDraggedEvent &e) {
        if (isResizeHandle(e.details)) {
            updateResize(e);
            return;
        }

        NonSimSceneComponent::onMouseDragged(e);
    }

    void ImageSceneComponent::onMouseDragEnd() {
        if (m_isResizing) {
            (void)Edit::trackComp(*this,
                                  m_resizeScene,
                                  std::move(m_resizeBefore),
                                  "image-resize");
            m_isResizing = false;
            m_activeResizeHandle = 0;
            m_resizeAnchor = {0.f, 0.f};
            m_resizeParentOffset = {0.f, 0.f};
            m_resizeStartPosition = {0.f, 0.f, 0.f};
            m_resizeStartScale = {0.f, 0.f};
            m_resizeBefore = {};
            m_resizeScene = UUID::null;
            return;
        }

        NonSimSceneComponent::onMouseDragEnd();
    }

    bool ImageSceneComponent::onMouseButton(
        const Events::MouseButtonEvent &e) {
        if (e.button == Events::MouseButton::left &&
            e.action == Events::MouseClickAction::press &&
            isResizeHandle(e.details)) {
            m_activeResizeHandle = e.details;
            if (e.sceneState != nullptr) {
                e.sceneState->clearSelectedComponents();
                e.sceneState->addSelectedComponent(getUuid());
            }
            return true;
        }

        if (e.action == Events::MouseClickAction::release && !m_isResizing) {
            m_activeResizeHandle = 0;
        }

        return false;
    }

    bool ImageSceneComponent::onMouseEnter(const Events::MouseEnterEvent &e) {
        m_hoveredHandle = isResizeHandle(e.details) ? e.details : 0;
        return true;
    }

    bool ImageSceneComponent::onMouseLeave(const Events::MouseLeaveEvent &e) {
        (void)e;
        m_hoveredHandle = 0;
        return true;
    }

    Core::Viewport::SceneCursor ImageSceneComponent::getCursor() const {
        if (m_isResizing && isResizeHandle(m_activeResizeHandle)) {
            return cursorForHandle(m_activeResizeHandle);
        }
        if (isResizeHandle(m_hoveredHandle)) {
            return cursorForHandle(m_hoveredHandle);
        }
        return Core::Viewport::SceneCursor::move;
    }

    void ImageSceneComponent::onDataChange() {
        clearTexture();
        m_textureDirty = true;
    }

    void ImageSceneComponent::onImageSizeChange() {
        clearTexture();
        m_textureDirty = true;
    }

    void ImageSceneComponent::resetCloneRuntimeState() {
        clearTexture();
        m_textureDirty = true;
        m_isResizing = false;
        m_activeResizeHandle = 0;
        m_hoveredHandle = 0;
        m_resizeBefore = {};
        m_resizeScene = UUID::null;
    }

    glm::vec2 ImageSceneComponent::calculateScale(const SceneState &state) {
        (void)state;
        if (m_imageWidth == 0u || m_imageHeight == 0u) {
            return {kDefaultImageSide, kDefaultImageSide};
        }

        glm::vec2 size{static_cast<float>(m_imageWidth),
                       static_cast<float>(m_imageHeight)};
        const float largestSide = std::max(size.x, size.y);
        if (largestSide > kMaxInitialImageSide) {
            size *= kMaxInitialImageSide / largestSide;
        }
        return sanitizedScale(size);
    }

    bool ImageSceneComponent::hasValidImagePayload() const {
        if (m_imageWidth == 0u || m_imageHeight == 0u ||
            m_imageData.empty()) {
            return false;
        }

        const auto width = static_cast<size_t>(m_imageWidth);
        const auto height = static_cast<size_t>(m_imageHeight);
        if (width > std::numeric_limits<size_t>::max() / height ||
            width * height >
                std::numeric_limits<size_t>::max() / static_cast<size_t>(4u)) {
            return false;
        }

        const auto expected = width * height * 4u;
        return m_imageData.size() >= expected;
    }

    bool ImageSceneComponent::hasValidSourceAspectRatio() const {
        return m_imageWidth > 0u && m_imageHeight > 0u;
    }

    float ImageSceneComponent::sourceAspectRatio() const {
        if (!hasValidSourceAspectRatio()) {
            return 1.f;
        }

        return static_cast<float>(m_imageWidth) /
               static_cast<float>(m_imageHeight);
    }

    glm::vec2 ImageSceneComponent::scaleMaintainingAspect(
        glm::vec2 scale,
        bool preferWidth) const {
        scale = sanitizedScale(scale);
        if (!hasValidSourceAspectRatio()) {
            return scale;
        }

        const float aspect = sourceAspectRatio();
        if (preferWidth) {
            scale.y = scale.x / aspect;
            if (scale.y < kMinResizeSide) {
                scale.y = kMinResizeSide;
                scale.x = scale.y * aspect;
            }
        } else {
            scale.x = scale.y * aspect;
            if (scale.x < kMinResizeSide) {
                scale.x = kMinResizeSide;
                scale.y = scale.x / aspect;
            }
        }

        return sanitizedScale(scale);
    }

    glm::vec2 ImageSceneComponent::scaleFromPropertiesEdit(
        glm::vec2 prevScale,
        glm::vec2 nextScale) const {
        nextScale = sanitizedScale(nextScale);
        if (!m_maintainAspectRatio || !hasValidSourceAspectRatio()) {
            return nextScale;
        }

        const auto delta = glm::abs(nextScale - sanitizedScale(prevScale));
        return scaleMaintainingAspect(nextScale, delta.x >= delta.y);
    }

    glm::vec2
    ImageSceneComponent::scaleFromResizeDrag(glm::vec2 rawScale) const {
        rawScale = sanitizedScale(rawScale);
        if (!m_maintainAspectRatio || !hasValidSourceAspectRatio()) {
            return {snappedSize(rawScale.x), snappedSize(rawScale.y)};
        }

        const auto delta = glm::abs(rawScale - m_resizeStartScale);
        const bool preferWidth = delta.x >= delta.y;
        if (preferWidth) {
            rawScale.x = snappedSize(rawScale.x);
        } else {
            rawScale.y = snappedSize(rawScale.y);
        }

        return scaleMaintainingAspect(rawScale, preferWidth);
    }

    void ImageSceneComponent::ensureTexture() {
        if (!m_textureDirty) {
            return;
        }

        clearTexture();
        m_textureDirty = false;

        if (!hasValidImagePayload()) {
            return;
        }

        try {
            m_texture = Wgpu::WgpuTexture::fromPixels(
                m_imageData.data(), m_imageWidth, m_imageHeight);
            m_textureHandle = m_texture ? m_texture->getHandle() : 0;
        } catch (const std::exception &ex) {
            BESS_ERROR("[ImageSceneComponent] Failed to create texture: {}",
                       ex.what());
            clearTexture();
        }
    }

    void ImageSceneComponent::clearTexture() {
        m_textureHandle = 0;
        m_texture.reset();
    }

    void ImageSceneComponent::drawImageQuad(SceneDrawContext &context,
                                            const glm::vec3 &pos,
                                            const PickingId &pickingId) {
        ensureTexture();

        if (m_textureHandle == 0) {
            drawPlaceholder(context, pos, pickingId);
            return;
        }

        Core::Renderer::QuadProps props;
        props.position = {pos.x, pos.y};
        props.size = sanitizedScale(m_transform.scale);
        props.zIndex = pos.z;
        props.color = Core::Renderer::Color{1.f, 1.f, 1.f, 1.f};
        props.texture = m_textureHandle;
        props.id = pickingId;
        props.transformMode = context.transformMode;
        context.renderer->drawQuad(props);
    }

    void ImageSceneComponent::drawPlaceholder(
        SceneDrawContext &context,
        const glm::vec3 &pos,
        const PickingId &pickingId) const {
        SceneDraw::QuadStyle backgroundStyle;
        backgroundStyle.borderColor =
            m_isSelected ? ViewportTheme::colors.selectedComp
                         : ViewportTheme::colors.componentBorder;
        backgroundStyle.borderSize = glm::vec4(1.f);
        backgroundStyle.borderRadius = glm::vec4(4.f);

        SceneDraw::drawQuad(context,
                            pos,
                            sanitizedScale(m_transform.scale),
                            ViewportTheme::colors.componentBG,
                            pickingId,
                            backgroundStyle);

        const auto half = sanitizedScale(m_transform.scale) * 0.5f;
        const auto inset = glm::vec2(std::min(half.x, half.y) * 0.30f);
        const auto color = ViewportTheme::colors.componentBorder;
        const float z = pos.z + 0.001f;
        SceneDraw::drawLine(context,
                            {pos.x - half.x + inset.x,
                             pos.y - half.y + inset.y,
                             z},
                            {pos.x + half.x - inset.x,
                             pos.y + half.y - inset.y,
                             z},
                            1.5f,
                            color,
                            pickingId);
        SceneDraw::drawLine(context,
                            {pos.x - half.x + inset.x,
                             pos.y + half.y - inset.y,
                             z},
                            {pos.x + half.x - inset.x,
                             pos.y - half.y + inset.y,
                             z},
                            1.5f,
                            color,
                            pickingId);
    }

    void ImageSceneComponent::drawSelectionControls(SceneDrawContext &context,
                                                   const glm::vec3 &pos) {
        const float border = screenToWorld(kHandleBorderScreenSize,
                                           context.camera);
        SceneDraw::QuadStyle outlineStyle;
        outlineStyle.borderColor = ViewportTheme::colors.selectedComp;
        outlineStyle.borderSize = glm::vec4(border);
        SceneDraw::drawQuad(context,
                            {pos.x, pos.y, pos.z + 0.002f},
                            sanitizedScale(m_transform.scale),
                            {0.f, 0.f, 0.f, 0.f},
                            PickingId{m_runtimeId, kBodyInfo},
                            outlineStyle);

        const float handleSize = screenToWorld(kHandleScreenSize,
                                               context.camera);
        const float handleBorder = screenToWorld(kHandleBorderScreenSize,
                                                 context.camera);

        SceneDraw::QuadStyle handleStyle;
        handleStyle.borderColor = ViewportTheme::colors.selectedComp;
        handleStyle.borderSize = glm::vec4(handleBorder);
        handleStyle.borderRadius = glm::vec4(handleSize * 0.20f);

        for (const auto info : {kResizeTopLeftInfo,
                                kResizeTopRightInfo,
                                kResizeBottomRightInfo,
                                kResizeBottomLeftInfo}) {
            const auto handlePos = handlePosition(pos, info);
            SceneDraw::drawQuad(context,
                                {handlePos.x, handlePos.y, pos.z + 0.003f},
                                {handleSize, handleSize},
                                ViewportTheme::colors.background,
                                PickingId{m_runtimeId, info},
                                handleStyle);
        }
    }

    void ImageSceneComponent::beginResize(
        const Events::MouseDraggedEvent &e) {
        if (e.sceneState == nullptr) {
            return;
        }

        m_isResizing = true;
        m_activeResizeHandle = e.details;
        m_resizeStartPosition = m_transform.position;
        m_resizeStartScale = sanitizedScale(m_transform.scale);
        m_resizeBefore = toJson();
        m_resizeScene = e.sceneState->getSceneId();

        const auto absPos =
            getAbsolutePosition(*e.sceneState, e.isSchematicMode);
        const auto absCenter = glm::vec2(absPos.x, absPos.y);
        m_resizeParentOffset = absCenter - glm::vec2(m_transform.position);

        const auto half = m_resizeStartScale * 0.5f;
        switch (m_activeResizeHandle) {
        case kResizeTopLeftInfo:
            m_resizeAnchor = absCenter + half;
            break;
        case kResizeTopRightInfo:
            m_resizeAnchor = {absCenter.x - half.x, absCenter.y + half.y};
            break;
        case kResizeBottomRightInfo:
            m_resizeAnchor = absCenter - half;
            break;
        case kResizeBottomLeftInfo:
            m_resizeAnchor = {absCenter.x + half.x, absCenter.y - half.y};
            break;
        default:
            m_resizeAnchor = absCenter;
            break;
        }
    }

    void ImageSceneComponent::updateResize(
        const Events::MouseDraggedEvent &e) {
        if (e.sceneState == nullptr) {
            return;
        }

        if (!m_isResizing) {
            beginResize(e);
        }

        if (!m_isResizing) {
            return;
        }

        const auto sign = resizeSign(m_activeResizeHandle);
        if (sign.x == 0.f || sign.y == 0.f) {
            return;
        }

        const auto pointer = e.mousePos;
        glm::vec2 newScale =
            scaleFromResizeDrag({sign.x * (pointer.x - m_resizeAnchor.x),
                                 sign.y * (pointer.y - m_resizeAnchor.y)});

        const auto draggedCorner = m_resizeAnchor + (sign * newScale);
        const auto newAbsCenter = (m_resizeAnchor + draggedCorner) * 0.5f;
        const auto newLocalCenter = newAbsCenter - m_resizeParentOffset;

        setScale(newScale);
        setPosition({newLocalCenter.x,
                     newLocalCenter.y,
                     m_resizeStartPosition.z});
    }

    bool ImageSceneComponent::isResizeHandle(uint32_t info) const {
        return info == kResizeTopLeftInfo || info == kResizeTopRightInfo ||
               info == kResizeBottomRightInfo ||
               info == kResizeBottomLeftInfo;
    }

    glm::vec2 ImageSceneComponent::handlePosition(const glm::vec3 &pos,
                                                  uint32_t info) const {
        const auto half = sanitizedScale(m_transform.scale) * 0.5f;
        switch (info) {
        case kResizeTopLeftInfo:
            return {pos.x - half.x, pos.y - half.y};
        case kResizeTopRightInfo:
            return {pos.x + half.x, pos.y - half.y};
        case kResizeBottomRightInfo:
            return {pos.x + half.x, pos.y + half.y};
        case kResizeBottomLeftInfo:
            return {pos.x - half.x, pos.y + half.y};
        default:
            return {pos.x, pos.y};
        }
    }

    Core::Viewport::SceneCursor
    ImageSceneComponent::cursorForHandle(uint32_t info) const {
        switch (info) {
        case kResizeTopLeftInfo:
        case kResizeBottomRightInfo:
            return Core::Viewport::SceneCursor::resizeDiagonalNWSE;
        case kResizeTopRightInfo:
        case kResizeBottomLeftInfo:
            return Core::Viewport::SceneCursor::resizeDiagonalNESW;
        default:
            return Core::Viewport::SceneCursor::move;
        }
    }
} // namespace Bess::Canvas
