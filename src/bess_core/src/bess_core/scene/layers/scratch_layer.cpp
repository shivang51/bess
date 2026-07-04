#include "bess_core/scene/layers/scratch_layer.h"
#include "bess_core/scene/scene_draw_context.h"
#include "bess_core/scene/scene_ui/ui_scene_component.h"
#include "common/bess_uuid.h"

namespace Bess::Canvas {
    namespace {
        std::shared_ptr<UI::UISceneComponent> btnComp = nullptr;
    } // namespace

    void ScratchLayer::init(SceneLifecycleContext &ctx) {
        btnComp = Canvas::UI::ButtonComp::create("Button", []() {
            BESS_INFO("Button clicked!"); //
        });

        auto btnNode = ctx.sceneState->getUINodeRegistry()->addNode(UUID());
        btnComp->setUINode(btnNode);

        ctx.sceneState->addComponent(btnComp);
    }

    void ScratchLayer::update(TimeMs ts, SceneUpdateContext &ctx) {
    }

    void ScratchLayer::draw(SceneRenderContext &ctx) {
    }
} // namespace Bess::Canvas
