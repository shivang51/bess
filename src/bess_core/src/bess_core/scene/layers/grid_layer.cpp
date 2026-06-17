#include "bess_core/scene/layers/grid_layer.h"
#include "bess_core/scene/scene_layer.h"
#include "bess_core/settings/viewport_theme.h"

namespace Bess::Canvas {

    void GridLayer::destroy(SceneLifecycleContext &ctx) {
    }

    void GridLayer::update(TimeMs ts, SceneUpdateContext &ctx) {
    }

    void GridLayer::draw(SceneRenderContext &ctx) {
        ctx.renderer->drawCustomQuad(
            {.position = {0.f, 0.f},
             .size = ctx.camera->getSize(),
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
    }

    void GridLayer::init(SceneLifecycleContext &ctx) {
        BESS_ASSERT(ctx.renderer, "Renderer is null in GridLayer init");

        if (m_gridShader == 0) {
            m_gridShader = ctx.renderer->createCustomQuadShader({
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
      let big_grid = viewport_grid_line(world_pos, big_spacing, 1.5, zoom);

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
    }
} // namespace Bess::Canvas
