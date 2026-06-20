#include "bess_core/scene/layers/scratch_layer.h"
#include "bess_core/renderer/colors.h"
#include "bess_core/scene/scene_ui/layout.h"
#include <cstdint>

namespace Bess::Canvas {
    namespace {
        Bess::Canvas::UI::UINodeRegistry registry;
        UI::UINode *rootNode = nullptr;
        constexpr glm::vec2 size(100, 50);
        constexpr glm::vec4 padding(10, 5, 9, 6);
        constexpr glm::vec4 margin(2, 3, 4, 5);
        constexpr glm::vec2 wrapContentSize =
            glm::vec2(padding.x + padding.z, padding.y + padding.w) +
            glm::vec2(margin.x + margin.z, margin.y + margin.w);

        void drawNode(const UI::UINode &node, SceneRenderContext &ctx) {

            uint64_t nodeId = node.getId();

            ctx.renderer->drawQuad({
                .position = node.getCachedPos(),
                .size = node.getDrawSize(),
                .color = Core::Renderer::Color::fromHex((uint32_t)nodeId),
            });
        }
    } // namespace

    void ScratchLayer::init(SceneLifecycleContext &ctx) {
        Bess::Canvas::UI::UINode node;
        node.setSize(size);
        node.setPadding(padding);
        node.setMargin(margin);
        node.setSizeConstraint(Bess::Canvas::UI::SizeContraint::wrap_content);
        node.setAlignment(Bess::Canvas::UI::LayoutAlignment::center);

        {
            Bess::Canvas::UI::UINode childNode1;
            childNode1.setSize(glm::vec2(50, 50));
            childNode1.setMargin(glm::vec4(5, 50, 5, 5));
            registry.addNode(childNode1);
            node.addChild(childNode1.getId());
        }

        {
            Bess::Canvas::UI::UINode childNode2;
            childNode2.setSize(glm::vec2(30, 30));
            registry.addNode(childNode2);
            node.addChild(childNode2.getId());
        }

        registry.addNode(node);
        rootNode = registry.getNode(node.getId());
    }

    void ScratchLayer::update(TimeMs ts, SceneUpdateContext &ctx) {
        // rootNode->measure(registry, Bess::UUID::null);
        // rootNode->layout(registry, Bess::UUID::null);
    }

    void ScratchLayer::draw(SceneRenderContext &ctx) {
        // for (const auto &node : registry.getAllNodes()) {
        //     drawNode(node.second, ctx);
        // }
    }
} // namespace Bess::Canvas
