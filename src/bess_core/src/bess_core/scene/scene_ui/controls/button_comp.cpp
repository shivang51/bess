#include "bess_core/scene/scene_ui/controls/button_comp.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/scene_state/scene_state.h"

namespace Bess::Canvas::UI {
    std::shared_ptr<ButtonComp>
    ButtonComp::create(const std::string &label,
                       const UIButtonCallback &callback) {
        auto button = std::make_shared<ButtonComp>();
        button->setName(label);
        button->setCallback(callback);
        return button;
    }

    bool ButtonComp::onMouseButton(const Events::MouseButtonEvent &e) {
        if (e.button != Events::MouseButton::left) {
            return false;
        }

        if (e.action == Events::MouseClickAction::press) {
            if (m_callback) {
                m_callback();
            }
            return true;
        }

        return e.action == Events::MouseClickAction::release;
    }

    void ButtonComp::update(TimeMs ts, SceneState &state) {
        UISceneComponent::update(ts, state);
    }

    void ButtonComp::onDraw(SceneDrawContext &state) {
        drawBgQuad(state);
        drawText(state, m_name, m_labelNode);
    }

    void ButtonComp::prepareUI(SceneUIPrepareCtx &state) {
        prepStyle(state.theme);
        initNode(state.sceneState->getUINodeRegistry());

        if (m_labelNode == nullptr) {
            m_labelNode =
                state.sceneState->getUINodeRegistry()->addNode(UUID());
        }

        const auto size = state.renderer->measureText(
            getName(),
            {
                .fontSize = m_style.textStyle.fontSize,
            });

        m_labelNode->setWidth(size.x);
        m_labelNode->setHeight(size.y);

        m_node->addChild(m_labelNode);

        m_node->setPadding(m_style.metrics.padding);
        m_node->setMargin(m_style.metrics.margin);

        if (state.parentNode != nullptr) {
            state.parentNode->addChild(m_node);
        }

        m_isUIDirty = false;
    }
} // namespace Bess::Canvas::UI
