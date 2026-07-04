#include "bess_core/scene/layers/scratch_layer.h"
#include "bess_core/scene/scene_draw_context.h"
#include "bess_core/scene/scene_ui/ui_scene_component.h"
#include "common/bess_uuid.h"

namespace Bess::Canvas {
    namespace {
        std::shared_ptr<UI::ContainerComp> container = nullptr;
    } // namespace

    void ScratchLayer::init(SceneLifecycleContext &ctx) {
        container = Canvas::UI::ContainerComp::create();
        ctx.sceneState->addComponent(container);
        auto containerNode =
            ctx.sceneState->getUINodeRegistry()->addNode(container->getUuid());
        container->setUINode(containerNode);

        for (int i = 0; i < 3; ++i) {
            auto btnComp = Canvas::UI::ButtonComp::create("Button", [i]() {
                if (i == 0) {
                    container->setDirection(
                        Canvas::UI::LayoutDirection::horizontal);
                } else {
                    container->setDirection(
                        Canvas::UI::LayoutDirection::vertical);
                }
            });

            btnComp->getStyle().margin = glm::vec4(5);

            auto btnNode = ctx.sceneState->getUINodeRegistry()->addNode(
                btnComp->getUuid());
            btnComp->setUINode(btnNode);
            containerNode->addChild(btnNode);

            ctx.sceneState->addComponent(btnComp);

            ctx.sceneState->attachChild(container->getUuid(),
                                        btnComp->getUuid());
        }
    }

    void ScratchLayer::update(TimeMs ts, SceneUpdateContext &ctx) {
    }

    void ScratchLayer::draw(SceneRenderContext &ctx) {
    }
} // namespace Bess::Canvas
