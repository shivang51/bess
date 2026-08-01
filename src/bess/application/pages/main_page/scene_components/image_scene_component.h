#pragma once

#include "common/bess_api.h"

#include "bess_core/renderer/texture.h"
#include "bess_core/scene/scene_draw_context.h"
#include "pages/main_page/scene_components/non_sim_scene_component.h"
#include <cstdint>
#include <memory>
#include <typeindex>
#include <vector>

#define IMAGE_SER_PROPS                                                        \
    ("imageData", getData, setData),                                           \
        ("imageWidth", getImageWidth, setImageWidth),                          \
        ("imageHeight", getImageHeight, setImageHeight),                       \
        ("maintainAspectRatio",                                                \
         getMaintainAspectRatio,                                               \
         setMaintainAspectRatio)

namespace Bess::Canvas {
    class BESS_API ImageSceneComponent : public NonSimSceneComponent {
      public:
        ImageSceneComponent();

        REG_SCENE_COMP_TYPE("ImageSceneComponent",
                            SceneComponentType::nonSimulation)

        SCENE_COMP_SER(Bess::Canvas::ImageSceneComponent,
                       Bess::Canvas::NonSimSceneComponent,
                       IMAGE_SER_PROPS)

        MAKE_GETTER_SETTER_WC(std::vector<uint8_t>,
                              Data,
                              m_imageData,
                              onDataChange)
        void setData(std::vector<uint8_t> &&value);
        MAKE_GETTER_SETTER_WC(uint32_t,
                              ImageWidth,
                              m_imageWidth,
                              onImageSizeChange)
        MAKE_GETTER_SETTER_WC(uint32_t,
                              ImageHeight,
                              m_imageHeight,
                              onImageSizeChange)
        MAKE_GETTER_SETTER(bool, MaintainAspectRatio, m_maintainAspectRatio)

        std::vector<std::shared_ptr<SceneComponent>>
        clone(const SceneState &sceneState) const override;

        void draw(SceneDrawContext &context) override;
        void drawSchematic(SceneDrawContext &context) override;

        std::type_index getTypeIndex() override;

        [[nodiscard]] Json::Value toEditJson() const override;
        [[nodiscard]] std::size_t
        estimatedMemoryUsage() const noexcept override;

        void drawPropertiesUI(SceneState &sceneState) override;
        void onMouseDragged(const Events::MouseDraggedEvent &e) override;
        void onMouseDragEnd() override;
        bool onMouseButton(const Events::MouseButtonEvent &e) override;
        bool onMouseEnter(const Events::MouseEnterEvent &e) override;
        bool onMouseLeave(const Events::MouseLeaveEvent &e) override;
        Core::Viewport::SceneCursor getCursor() const override;

      private:
        void onDataChange();
        void onImageSizeChange();
        void resetCloneRuntimeState() override;
        glm::vec2 calculateScale(const SceneState &state) override;

        bool hasValidImagePayload() const;
        bool hasValidSourceAspectRatio() const;
        float sourceAspectRatio() const;
        glm::vec2 scaleMaintainingAspect(glm::vec2 scale,
                                         bool preferWidth) const;
        glm::vec2 scaleFromPropertiesEdit(glm::vec2 prevScale,
                                          glm::vec2 nextScale) const;
        glm::vec2 scaleFromResizeDrag(glm::vec2 rawScale) const;
        void ensureTexture();
        void clearTexture();
        void drawImageQuad(SceneDrawContext &context,
                           const glm::vec3 &pos,
                           const PickingId &pickingId);
        void drawPlaceholder(SceneDrawContext &context,
                             const glm::vec3 &pos,
                             const PickingId &pickingId) const;
        void drawSelectionControls(SceneDrawContext &context,
                                   const glm::vec3 &pos);
        void beginResize(const Events::MouseDraggedEvent &e);
        void updateResize(const Events::MouseDraggedEvent &e);
        bool isResizeHandle(uint32_t info) const;
        glm::vec2 handlePosition(const glm::vec3 &pos, uint32_t info) const;
        Core::Viewport::SceneCursor cursorForHandle(uint32_t info) const;

      private:
        std::vector<uint8_t> m_imageData;
        uint32_t m_imageWidth = 0;
        uint32_t m_imageHeight = 0;
        bool m_maintainAspectRatio = true;
        std::shared_ptr<Core::Renderer::ITexture> m_texture = nullptr;
        Core::Renderer::TextureHandle m_textureHandle = 0;
        bool m_textureDirty = true;

        bool m_isResizing = false;
        uint32_t m_activeResizeHandle = 0;
        uint32_t m_hoveredHandle = 0;
        glm::vec2 m_resizeAnchor{0.f};
        glm::vec2 m_resizeParentOffset{0.f};
        glm::vec3 m_resizeStartPosition{0.f};
        glm::vec2 m_resizeStartScale{0.f};
        Json::Value m_resizeBefore;
        UUID m_resizeScene = UUID::null;
    };
} // namespace Bess::Canvas

REFLECT_DERIVED_PROPS(Bess::Canvas::ImageSceneComponent,
                      Bess::Canvas::NonSimSceneComponent,
                      IMAGE_SER_PROPS);
