#include "bess_core/scene/layers/scratch_layer.h"
#include "bess_core/scene/scene_draw_context.h"
#include "bess_core/scene/scene_ui/ui_scene_component.h"

namespace Bess::Canvas {
    namespace {
        std::shared_ptr<UI::ContainerComp> container = nullptr;
    } // namespace

    void ScratchLayer::init(SceneLifecycleContext &ctx) {
        container = Canvas::UI::ContainerComp::create();
        ctx.sceneState->addComponent(container);
        container->setDrawBackground(true);

        for (int i = 0; i < 3; ++i) {
            auto btnComp = Canvas::UI::ButtonComp::create(
                std::format("Button {}", i), [i]() {
                    if (i == 0) {
                        container->setDirection(
                            Canvas::UI::LayoutDirection::horizontal);
                    } else if (i == 1) {
                        container->setDirection(
                            Canvas::UI::LayoutDirection::vertical);
                    } else if (i == 2) {
                        container->setDrawBackground(
                            !container->getDrawBackground());
                    }
                });

            btnComp->getStyle().margin = 5;

            ctx.sceneState->addComponent(btnComp);

            ctx.sceneState->attachChild(container->getUuid(),
                                        btnComp->getUuid());
        }

        auto toggle = Canvas::UI::ToggleBtnComp::create("Toggle Me", false);
        toggle->setShowLabel(true);
        toggle->getStyle().margin = 5;

        container->addChildComponent(toggle->getUuid());
        ctx.sceneState->addComponent(toggle);
    }

    void ScratchLayer::update(TimeMs ts, SceneUpdateContext &ctx) {
    }

    void ScratchLayer::draw(SceneRenderContext &ctx) {
    }
} // namespace Bess::Canvas
