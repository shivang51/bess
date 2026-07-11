#pragma once

#include "bess_core/scene/scene_layer.h"
#include "bess_core/scene/scene_ui/controls/container_comp.h"
#include "bess_core/scene/scene_ui/controls/label_comp.h"
#include "bess_core/viewport.h"
#include <functional>
#include <vector>

namespace Bess::Canvas {
    class ScreenSpaceOverlayLayer : public ISceneLayer {
      public:
        using DrawCallback =
            std::function<void(SceneDrawContext &, SceneRenderContext &)>;

        void update(TimeMs ts, SceneUpdateContext &ctx) override;
        void viewportUpdate(TimeMs ts, SceneVpUpdateContext &ctx) override;
        void draw(SceneRenderContext &ctx) override;
        void reset(SceneLifecycleContext &ctx) override;
        void destroy(SceneLifecycleContext &ctx) override;
        void init(SceneLifecycleContext &ctx) override;

        void addDrawCallback(DrawCallback callback);
        void clearDrawCallbacks();

        std::string getName() const override {
            return "ScreenSpaceOverlayLayer";
        }

      private:
        bool updateTransform(
            const std::shared_ptr<Core::Viewport::ViewportContext> &ctx);

        void fmtCamPos(const glm::vec2 &pos);
        void fmtCamZoom(float zoom);

      private:
        std::vector<DrawCallback> m_drawCallbacks;
        std::shared_ptr<Bess::Canvas::UI::LabelComp> m_camPosXLabel,
            m_camPosYLabel, m_camZoomLabel;
        std::shared_ptr<Bess::Canvas::UI::ContainerComp> m_topContainer;
        std::shared_ptr<Bess::Canvas::UI::ContainerComp> m_bottomContainer;
        std::shared_ptr<Camera> m_camera;
        std::shared_ptr<Core::Viewport::ViewportContext> m_vpCtx = nullptr;
        bool m_updateTransforms = true;
    };
} // namespace Bess::Canvas
