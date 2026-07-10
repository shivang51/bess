#include "bess_core/scene/scene_ui/controls/toggle_btn_comp.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/scene_state/scene_state.h"

namespace Bess::Canvas::UI {
    std::shared_ptr<ToggleBtnComp>
    ToggleBtnComp::create(const std::string &label,
                          const ToggleBtnCallback &callback,
                          bool toggled) {
        auto toggleBtn = std::make_shared<ToggleBtnComp>();
        toggleBtn->m_name = label;
        toggleBtn->m_callback = callback;
        toggleBtn->m_toggled = toggled;
        return toggleBtn;
    }

    void ToggleBtnComp::onDraw(SceneDrawContext &state) {
        if (m_trackNode == nullptr || state.renderer == nullptr) {
            return;
        }

        if (m_showLabel) {
            drawText(state, m_name, m_labelNode);
        }

        Core::Renderer::QuadProps trackProps;
        trackProps.position = m_trackNode->getDrawPos();
        trackProps.size = m_trackNode->getDrawSize();
        trackProps.zIndex = m_trackNode->getDrawPos().z;
        trackProps.color = m_toggled ? m_trackOnColor : m_trackOffColor;
        trackProps.borderColor = m_style.borderColor;
        trackProps.thickness = m_style.metrics.borderSize.toVec4();
        trackProps.radius = m_style.metrics.borderRadius;
        trackProps.transformMode = state.transformMode;
        trackProps.id = PickingId{.runtimeId = resolveRuntimeId(), .info = 1};

        state.renderer->drawQuad(trackProps);
        trackProps.position.x += m_toggled
                                     ? (m_trackNode->getDrawSize().x / 2.f) -
                                           (m_thumbSize.x / 2.f) -
                                           (m_style.metrics.borderSize.left)
                                     : -(m_trackNode->getDrawSize().x / 2.f) +
                                           ((m_thumbSize.x / 2.f) +
                                            m_style.metrics.borderSize.right);
        trackProps.size = m_thumbSize;
        trackProps.zIndex += 0.0001f;
        trackProps.thickness = glm::vec4(0.f);
        trackProps.color = m_toggled ? m_thumbOnColor : m_thumbOffColor;
        state.renderer->drawQuad(trackProps);
    }

    bool ToggleBtnComp::onMouseButton(const Events::MouseButtonEvent &e) {
        if (e.action == Events::MouseClickAction::press &&
            e.button == Events::MouseButton::left && e.details == 1) {
            m_toggled = !m_toggled;
            if (m_callback) {
                m_callback(m_toggled);
            }
            return true;
        }

        return false;
    }

    void ToggleBtnComp::prepareUI(SceneUIPrepareCtx &state) {
        prepStyle(state.theme);
        initNode(state.sceneState->getUINodeRegistry());

        if (m_labelNode == nullptr || m_trackNode == nullptr) {
            m_labelNode =
                state.sceneState->getUINodeRegistry()->addNode(UUID());
            m_trackNode =
                state.sceneState->getUINodeRegistry()->addNode(UUID());
        }

        const auto &colors = state.theme->getColorScheme().getColors();
        m_style.metrics.borderSize = Core::Style::BorderSize(0.f);
        m_trackOffColor = colors.secondaryContainer;
        m_trackOnColor = colors.primary;
        m_thumbOffColor = colors.onSecondaryContainer;
        m_thumbOnColor = colors.onPrimary;

        m_node->setDirection(LayoutDirection::horizontal);
        m_node->setWidthFitContent();
        m_node->setHeightFitContent();
        m_node->setCrossAxisAlignment(LayoutAlignment::center);
        m_node->setPadding(m_style.metrics.padding);
        m_node->setMargin(m_style.metrics.margin);

        if (m_showLabel) {
            auto labelSize = state.renderer->measureText(
                m_name,
                {
                    .fontSize = m_style.textStyle.fontSize,
                });

            m_labelNode->setWidth(labelSize.x);
            m_labelNode->setHeight(labelSize.y);
            m_labelNode->setPosMode(PosMode::relative);
            m_labelNode->setPadding(0.f);
            m_labelNode->setMargin(
                Core::Style::Margin::onlyRight(m_labelTrackSpacing));

            m_node->addChild(m_labelNode);
        }

        m_node->addChild(m_trackNode);

        m_trackNode->setWidth(m_trackSize.x);
        m_trackNode->setHeight(m_trackSize.y);
        m_trackNode->setPosMode(PosMode::relative);
        m_trackNode->setPos(glm::vec2(0.f));
        m_trackNode->setPadding(0.f);
        m_trackNode->setMargin(0.f);

        if (state.parentNode != nullptr) {
            state.parentNode->addChild(m_node);
        }

        m_isUIDirty = false;
    }
} // namespace Bess::Canvas::UI
