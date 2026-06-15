#pragma once

#include "bess_core/renderer/renderer_types.h"
#include "common/bess_uuid.h"
#include "common/types.h"
#include "layers/screen_space_overlay_layer.h"
#include "scene/camera.h"
#include "scene/scene_events.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene/scene_state/scene_state.h"
#include "scene/scene_types.h"
#include "scene_layer.h"
#include "sim_driver/sim_driver.h"
#include <memory>

namespace Bess::Canvas {

    struct View2D {
        std::shared_ptr<Camera> camera;
        std::shared_ptr<Core::Renderer::IRenderer2D> renderer;
        Core::Renderer::TextureHandle drawRenderTarget;
        Core::Renderer::TextureHandle pickingRenderTarget;
        ViewportTransform viewportTransform;
        PickingId pickingId;
        SceneInputState &inputState;
    };

    class Scene {
      public:
        Scene();
        ~Scene();

      public:
        using ScreenOverlayDrawCallback = ScreenSpaceOverlayLayer::DrawCallback;

        void destroy();

        void reset();
        void clear();
        void update(TimeMs ts, bool isFocused);
        void viewportUpdate(TimeMs ts,
                            bool isFocused,
                            const ViewportTransform &viewportTransform,
                            SceneInputState &inputState,
                            const PickingId &pickingId);
        void draw(const View2D &view);
        void addScreenOverlayDrawCallback(ScreenOverlayDrawCallback callback);
        void clearScreenOverlayDrawCallbacks();

        const SceneState &getState() const;
        SceneState &getState();

        MAKE_GETTER_SETTER(bool, IsFirstFrame, m_isFirstFrame)

      public:
        const UUID &getSceneId() const;

        void addComponent(const std::shared_ptr<SceneComponent> &comp,
                          bool setZ = true);

        void setSceneMode(SceneMode mode);
        SceneMode getSceneMode() const;

        void resize(const glm::vec2 &size);
        const glm::vec2 &getSize() const;

        bool deleteSceneEntity(const UUID &entUuid);
        void deleteSelectedSceneEntities();

        void setZCoord(float val);

        bool getIsSchematicView() const;
        void setIsSchematicView(bool value);
        void toggleSchematicView();

        void selectAllEntities();
        void focusCameraOnSelected();
        glm::vec2 toScenePos(const glm::vec2 &mousePos) const;

        float getNextZCoord();

        bool dispatchEvent(SceneEvent &evt,
                           const ViewportTransform &viewportTransform,
                           const PickingId &pickingId,
                           SceneInputState &inputState);

        void onLeftMouse(bool isPressed,
                         const ViewportTransform &viewportTransform,
                         const PickingId &pickingId,
                         SceneInputState &inputState);

        void onMiddleMouse(bool isPressed,
                           const ViewportTransform &viewportTransform,
                           const PickingId &pickingId,
                           SceneInputState &inputState);

        void onMouseMove(const glm::vec2 &pos,
                         const ViewportTransform &viewportTransform,
                         const PickingId &pickingId,
                         SceneInputState &inputState);

      private:
        glm::vec2 getViewportMousePos(const glm::vec2 &mousePos,
                                      const glm::vec2 &viewportPos) const;

      private:
        glm::vec2 m_size;
        std::shared_ptr<Camera> m_camera = nullptr;

        SceneState m_state;

        SceneMode m_sceneMode = SceneMode::general;

        static constexpr float m_zIncrement = 0.001f;
        float m_compZCoord = m_zIncrement;

        TimeMs m_frameTimeStep = {};

        bool m_isDestroyed = false;
        bool m_isFirstFrame = true;

        std::vector<std::unique_ptr<ISceneLayer>> m_sceneLayers;
        ScreenSpaceOverlayLayer *m_screenSpaceOverlayLayer = nullptr;
    };
} // namespace Bess::Canvas
