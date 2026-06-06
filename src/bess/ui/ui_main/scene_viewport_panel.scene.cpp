#include "scene_viewport_panel.h"
#include "bess_core/g_app_context.h"
#include "common/types.h"
#include "events/application_event.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "renderer/vulkan/path_renderer.h"
#include "scene.h"
#include "scene/scene_draw_context.h"
#include "settings/viewport_theme.h"
#include "sub_systems/input_sub_system.h"
#include "sub_systems/renderer_context.h"
#include <cstdint>

namespace Bess::UI {
    uint64_t decodeGpuHoverValue(const glm::uvec2 &encodedId) {
        uint64_t id = static_cast<uint64_t>(encodedId.x);
        id |= (static_cast<uint64_t>(encodedId.y) << 32);
        return id;
    }

    void SceneViewportPanel::updateScene(TimeMs ts) {
        Canvas::ViewportTransform vpTrans{.pos = m_viewportPos,
                                          .size = m_viewportSize};
        m_attachedScene->updateViewportTransform(vpTrans);

        auto &appCtx = Bess::GAppContext::getInstance();
        auto inputSystem = appCtx.getSubSystem<InputSubSystem>();

        const auto &frameInputState = inputSystem->getFrameInpState();

        bool mouseMoved = false;
        if (frameInputState.hasMouseMoved) {
            mouseMoved = true;
            const auto &mouseMoveState = inputSystem->getMouseMoveState();
            ApplicationEvent::MouseMoveData data{
                mouseMoveState.pos.x,
                mouseMoveState.pos.y,
            };
            handleMouseMoveEvt(
                ApplicationEvent(ApplicationEventType::MouseMove, data));
        }

        auto mouseBtnState = frameInputState.mouseBtnState;
        const auto isInsideVp = isInsideViewport(mouseBtnState.pos);

        if (frameInputState.hasMouseBtnEvent && !isInsideVp &&
            mouseBtnState.action == MouseButtonAction::release) {
            // manually updating mouse state
            // because we don't update scene if its not active

            const auto isLeft = (mouseBtnState.button == MouseButton::left &&
                                 m_attachedScene->getIsLeftMousePressed());

            const auto isMiddle =
                (mouseBtnState.button == MouseButton::middle &&
                 m_attachedScene->getIsMiddleMousePressed());

            if (isLeft) {
                m_attachedScene->onLeftMouse(false);
            } else if (isMiddle) {
                m_attachedScene->onMiddleMouse(false);
            }
        }

        if (!m_attachedScene->getIsFirstFrame()) {
            updatePickingIds(mouseMoved);
        }
    }

    bool SceneViewportPanel::isInsideViewport(const glm::vec2 &pos) const {
        return pos.x >= m_viewportPos.x &&
               pos.x < m_viewportPos.x + m_viewportSize.x &&
               pos.y >= m_viewportPos.y &&
               pos.y < m_viewportPos.y + m_viewportSize.y;
    }

    void SceneViewportPanel::handleMouseMoveEvt(const ApplicationEvent &event) {
        const auto data = event.getData<ApplicationEvent::MouseMoveData>();

        const glm::vec2 mousePos(data.x, data.y);

        const bool isInsideVp = isInsideViewport(mousePos);

        auto window = Pages::MainPage::getInstance()->getParentWindow();

        if (isInsideVp) {
            window->setEnableCursor(true);
            return;
        }

        const bool isLeftPressed = m_attachedScene->getIsLeftMousePressed();

        if (isLeftPressed) {

            auto newPos = mousePos;

            auto vel = glm::vec2(0.f);
            if (mousePos.x < m_viewportPos.x) {
                vel.x = -10;
                newPos.x = m_viewportPos.x + 1.f;
            } else if (mousePos.x > m_viewportPos.x + m_viewportSize.x) {
                vel.x = 10;
                newPos.x = m_viewportPos.x + m_viewportSize.x - 1.f;
            } else if (mousePos.y < m_viewportPos.y) {
                vel.y = -10;
                newPos.y = m_viewportPos.y + 1.f;
            } else if (mousePos.y > m_viewportPos.y + m_viewportSize.y) {
                vel.y = 10;
                newPos.y = m_viewportPos.y + m_viewportSize.y - 1.f;
            }

            m_attachedScene->getCamera()->incrementPos(vel);

            window->setEnableCursor(false);
            window->setMousePos(newPos);

            m_attachedScene->onMouseMove({newPos.x, newPos.y});
        } else {
            window->setEnableCursor(true);
        }
    }

    void SceneViewportPanel::renderAttachedScene() {
        if (m_sceneTexture == nullptr || m_pickingTexture == nullptr) {
            return;
        }
        const auto &appCtx = GAppContext::getInstance();
        const auto &renderCtx = appCtx.getSubSystem<RendererContext>();
        const auto &sceneState = m_attachedScene->getState();

        const auto &renderer = renderCtx->getRenderer();

        renderer->beginFrame({
            .extent = {(uint32_t)m_viewportSize.x, (uint32_t)m_viewportSize.y},
            .clearColor = ViewportTheme::colors.background,
            .shouldClear = true,
            .targetTexture = m_sceneTexture->getHandle(),
            .pickingTexture = m_pickingTexture->getHandle(),
            .cameraTransform =
                glm::value_ptr(m_attachedScene->getCamera()->getTransform()),
        });

        if (m_gridShader == 0) {
            m_gridShader = renderer->createCustomQuadShader({
                .label = "viewport_grid",
                .fragmentSource = R"(
  fn viewport_grid_camera_offset(in: CustomQuadFragmentInput) -> vec2f {
      let sx = in.camera_transform[0][0];
      let sy = in.camera_transform[1][1];
      var camera_x = 0.0;
      var camera_y = 0.0;
      if (abs(sx) > 0.000001) {
          camera_x = -in.camera_transform[3][0] / sx;
      }
      if (abs(sy) > 0.000001) {
          camera_y = -in.camera_transform[3][1] / sy;
      }
      return vec2f(-camera_x, -camera_y);
  }

  fn viewport_grid_line(world_pos: vec2f, spacing: f32, thickness_px: f32,
                        zoom: f32) -> f32 {
      let spacing_vec = vec2f(spacing, spacing);
      var dist = abs(world_pos - floor(world_pos / spacing_vec) * spacing_vec);
      dist = min(dist, spacing_vec - dist);

      let thickness_world = thickness_px / zoom;
      let d = min(dist.x, dist.y);
      return smoothstep(thickness_world, 0.0, d);
  }

  fn custom_quad_fragment(in: CustomQuadFragmentInput) -> vec4f {
      let small_spacing = 10.0;
      let big_spacing = 100.0;
      let zoom = max(in.camera_zoom, 0.0001);
      let camera_offset = viewport_grid_camera_offset(in);
      let world_pos = ((in.frag_coord.xy - in.viewport * 0.5) / zoom) -
                      camera_offset;

      var small_grid = viewport_grid_line(world_pos, small_spacing, 1.0, zoom);
      let big_grid = viewport_grid_line(world_pos, big_spacing, 2.0, zoom);

      let small_fade = clamp((zoom - 0.5) * 2.0, 0.0, 1.0);
      small_grid *= small_fade;

      let intensity = max(small_grid * 0.3, big_grid * 0.6);
      if (intensity <= 0.0001) {
          discard;
      }

      var grid_color = in.data1;
      if (small_grid >= big_grid) {
          grid_color = in.data0;
      }

      let axis_thickness_world = 2.0 / zoom;
      if (abs(world_pos.x) < axis_thickness_world) {
          grid_color = in.data3;
      }
      if (abs(world_pos.y) < axis_thickness_world) {
          grid_color = in.data2;
      }

      return grid_color * in.color;
  }
  )",
            });
        }

        renderer->drawCustomQuad(
            {.position = {0.f, 0.f},
             .size = m_viewportSize,
             .zIndex = -10000.f,
             .color = {1.f, 1.f, 1.f, 1.f},
             .id = PickingId::invalid(),
             .renderPass = Core::Renderer::QuadRenderPass::Opaque},
            m_gridShader,
            {ViewportTheme::colors.gridMinorColor,
             ViewportTheme::colors.gridMajorColor,
             ViewportTheme::colors.gridAxisXColor,
             ViewportTheme::colors.gridAxisYColor},
            Core::Renderer::CustomQuadTransformMode::Screen);

        renderer->drawFont("Hello World", {
                                              .position = {100.f, 100.f},
                                              .fontSize = 12.f,
                                              .color = {1.f, 1.f, 0.f, 1.f},
                                              .id = PickingId{2, 0},
                                          });
        renderer->drawFont("Hello World", {
                                              .position = {100.f, 120.f},
                                              .fontSize = 12.f,
                                              .color = {1.f, 1.f, 0.f, 1.f},
                                              .id = PickingId{3, 0},
                                          });

        if (m_uvDebugShader == 0) {
            m_uvDebugShader = renderer->createCustomQuadShader({
                .label = "uv_debug",
                .fragmentSource = R"(
  fn custom_quad_fragment(in: CustomQuadFragmentInput) -> vec4f {
      return vec4f(in.uv, 0.5 + 0.5 * sin(in.data0.x), 1.0) * in.color;
  }
  )",
            });
            m_uvDebugStartTime = std::chrono::steady_clock::now();
        }

        const float timeSeconds =
            std::chrono::duration<float>(std::chrono::steady_clock::now() -
                                         m_uvDebugStartTime)
                .count();

        renderer->drawCustomQuad({.size = {400, 400}, .id = PickingId{1, 0}},
                                 m_uvDebugShader,
                                 {
                                     glm::vec4(timeSeconds, 0.f, 0.f, 0.f),
                                 },
                                 Core::Renderer::CustomQuadTransformMode::
                                     Screen);

        renderer->endFrame();
        m_attachedScene->setIsFirstFrame(false);
    }

    void SceneViewportPanel::drawGrid(SceneDrawContext &context) {
        context.materialRenderer->drawGrid(
            glm::vec3(0.f, 0.f, 0.1f), context.camera->getSpan(),
            PickingId::invalid(),
            {
                .minorColor = ViewportTheme::colors.gridMinorColor,
                .majorColor = ViewportTheme::colors.gridMajorColor,
                .axisXColor = ViewportTheme::colors.gridAxisXColor,
                .axisYColor = ViewportTheme::colors.gridAxisYColor,
            },
            context.camera);
    }

    void SceneViewportPanel::drawGhostConnection(
        const std::shared_ptr<Renderer::PathRenderer> &pathRenderer,
        const glm::vec2 &startPos, const glm::vec2 &endPos) {
        auto midX = (startPos.x + endPos.x) / 2.f;

        const auto &id = PickingId::invalid();

        pathRenderer->beginPathMode(glm::vec3(startPos.x, startPos.y, 0.8f),
                                    2.f, ViewportTheme::colors.ghostWire, id);

        pathRenderer->pathLineTo(glm::vec3(midX, startPos.y, 0.8f), 2.f,
                                 ViewportTheme::colors.ghostWire, id);

        pathRenderer->pathLineTo(glm::vec3(midX, endPos.y, 0.8f), 2.f,
                                 ViewportTheme::colors.ghostWire, id);

        pathRenderer->pathLineTo(glm::vec3(endPos, 0.8f), 2.f,
                                 ViewportTheme::colors.ghostWire, id);

        pathRenderer->endPathMode(false, false, glm::vec4(1.f), true, true);
    }

    void SceneViewportPanel::drawComponents(SceneDrawContext &context) {
        auto &sceneState = *context.sceneState;
        const auto &cam = context.camera;
        const auto &span = (cam->getSpan() / 2.f) + 200.f;
        const auto &camPos = cam->getPos();

        for (const auto &compId : sceneState.getRootComponents()) {
            const auto comp = sceneState.getComponentByUuid(compId);

            const auto &pos = comp->getAbsolutePosition(sceneState);
            const auto x = pos.x - camPos.x;
            const auto y = pos.y - camPos.y;

            // skipping if outside camera and not connection
            // Connections are exempted
            if (comp->getType() != Canvas::SceneComponentType::connection &&
                (x < -span.x || x > span.x || y < -span.y || y > span.y))
                continue;

            if (sceneState.getIsSchematicView()) {
                comp->drawSchematic(context);
            } else {
                comp->draw(context);
            }
        }
    }

    void SceneViewportPanel::drawSelectionBox(SceneDrawContext &context) {
        const auto &state = *context.sceneState;
        const auto &selCtx = m_attachedScene->getSelBoxContext();

        const auto start = m_attachedScene->toScenePos(selCtx.start);
        const auto end =
            m_attachedScene->toScenePos(m_attachedScene->getMousePos());

        auto size = end - start;
        const auto pos = start + (size / 2.f);
        size = glm::abs(size);

        Renderer::QuadRenderProperties props;
        props.borderColor = ViewportTheme::colors.selectionBoxBorder;
        props.borderSize = glm::vec4(1.f);

        context.materialRenderer->drawQuad(
            glm::vec3(pos, 7.f), size, ViewportTheme::colors.selectionBoxFill,
            -1, props);
    }

    void SceneViewportPanel::updatePickingIds(bool mouseMoved) {
        const auto &renderer = GAppContext::getInstance()
                                   .getSubSystem<RendererContext>()
                                   ->getRenderer();

        Core::Renderer::PickingReadbackResult pickingResult;
        if (renderer->tryGetPickingIds(pickingResult) &&
            !pickingResult.empty()) {
            m_attachedScene->setPickingId(pickingResult.firstOrInvalid());
        }

        if (!mouseMoved || m_pickingTexture == nullptr ||
            m_pickingTexture->getHandle() == 0) {
            return;
        }

        const glm::vec2 mousePos = m_attachedScene->getMousePos();
        const glm::vec2 textureSize = m_pickingTexture->getSize();
        const uint32_t width =
            textureSize.x > 1.f ? static_cast<uint32_t>(textureSize.x) : 1u;
        const uint32_t height =
            textureSize.y > 1.f ? static_cast<uint32_t>(textureSize.y) : 1u;

        if (mousePos.x < 0.f || mousePos.y < 0.f ||
            mousePos.x >= static_cast<float>(width) ||
            mousePos.y >= static_cast<float>(height)) {
            m_attachedScene->setPickingId(PickingId::invalid());
            return;
        }

        renderer->requestPickingId(m_pickingTexture->getHandle(),
                                   static_cast<uint32_t>(mousePos.x),
                                   static_cast<uint32_t>(mousePos.y));
    }
} // namespace Bess::UI
