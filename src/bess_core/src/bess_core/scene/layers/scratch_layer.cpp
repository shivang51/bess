#include "bess_core/scene/layers/scratch_layer.h"
#include "bess_core/scene/scene_draw_context.h"
#include "bess_core/scene/scene_ui/controls/button_comp.h"
#include "bess_core/scene/scene_ui/controls/container_comp.h"
#include "bess_core/scene/scene_ui/controls/text_box_comp.h"
#include "bess_core/scene/scene_ui/controls/toggle_btn_comp.h"

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

        auto toggle = Canvas::UI::ToggleBtnComp::create("Toggle Me");

        toggle->setCallback([toggle](bool toggled) {
            toggle->setName(toggled ? "Toggled On" : "Toggled Off");
        });
        toggle->setShowLabel(true);
        toggle->getStyle().margin = 5;

        container->addChildComponent(toggle->getUuid());
        ctx.sceneState->addComponent(toggle);

        auto textBox = Canvas::UI::TextBoxComp::create("Type here...");
        textBox->setChangedCallback([](const std::string &value) {
            BESS_INFO("Text changed: {}", value);
        });

        container->addChildComponent(textBox->getUuid());
        ctx.sceneState->addComponent(textBox);
    }

    void ScratchLayer::update(TimeMs ts, SceneUpdateContext &ctx) {
    }

    void ScratchLayer::draw(SceneRenderContext &ctx) {
    }
} // namespace Bess::Canvas
