#include "bess_core/scene/layers/scratch_layer.h"

namespace Bess::Canvas {

    void ScratchLayer::update(TimeMs ts, SceneUpdateContext &ctx) {
    }

    void ScratchLayer::draw(SceneRenderContext &ctx) {
        ctx.renderer->drawQuad({
            .position = {0.f, 0.f},
            .size = glm::vec2(100, 100),
            .color = {0.1f, 0.1f, 0.1f, 1.f},
            .renderPass = Core::Renderer::QuadRenderPass::Opaque,
        });
    }
} // namespace Bess::Canvas
