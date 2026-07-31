#include "bess_core/scene/scene_ui/controls/selectable_button_comp.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/scene_event.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include <algorithm>

namespace Bess::Canvas::UI {
    namespace {
        constexpr uint32_t kButtonInfo = 1u;
        constexpr float kMinButtonWidth = 32.f;
        constexpr float kMinButtonHeight = 18.f;
    } // namespace

    std::shared_ptr<SelectableButtonComp>
    SelectableButtonComp::create(const CompConfig &config) {
        return create("", nullptr, false, config);
    }

    std::shared_ptr<SelectableButtonComp>
    SelectableButtonComp::create(const std::string &label,
                                 const UISelectableButtonCallback &callback,
                                 bool selected,
                                 const CompConfig &config) {
        auto button = std::make_shared<SelectableButtonComp>();
        button->setName(label);
        button->setCallback(callback);
        button->setSelected(selected);
        applyCompConfig(button, config);
        return button;
    }

    bool SelectableButtonComp::getSelected() const noexcept {
        return m_selected;
    }

    void SelectableButtonComp::setSelected(bool selected) noexcept {
        m_selected = selected;
    }

    void SelectableButtonComp::toggleSelected() {
        setSelected(!m_selected);
    }

    void SelectableButtonComp::onDraw(SceneDrawContext &state) {
        if (m_node == nullptr || state.renderer == nullptr) {
            return;
        }

        const PickingId id{
            .runtimeId = resolveRuntimeId(),
            .info = kButtonInfo,
        };

        Core::Renderer::QuadProps props;
        props.position = m_node->getDrawPos();
        props.size = m_node->getDrawSize();
        props.zIndex = m_node->getDrawPos().z;
        props.color = m_selected  ? m_style.activeColor
                      : m_hovered ? m_style.hoverColor
                                  : m_style.backgroundColor;
        props.borderColor = (m_focused || m_selected) ? m_style.activeColor
                                                      : m_style.borderColor;
        props.thickness = m_style.metrics.borderSize.toVec4();
        props.radius = m_style.metrics.borderRadius;
        props.id = id;
        props.transformMode = state.transformMode;
        props.shadow = m_style.shadowProps;
        state.renderer->drawQuad(props);

        drawLabel(state);
    }

    void SelectableButtonComp::prepareUI(SceneUIPrepareCtx &state) {
        prepStyle(state.theme);
        initNode(state.sceneState->getUINodeRegistry());

        if (m_labelNode == nullptr) {
            m_labelNode =
                state.sceneState->getUINodeRegistry()->addNode(UUID());
        }

        const auto size = resolveButtonSize(state);
        const auto labelSize = state.renderer->measureText(
            getName(),
            {
                .fontSize = m_style.textStyle.fontSize,
            });

        m_node->setDirection(LayoutDirection::horizontal);
        m_node->setWidth(size.x);
        m_node->setHeight(size.y);
        m_node->setCrossAxisAlignment(LayoutAlignment::center);
        m_node->setPadding(m_style.metrics.padding);
        m_node->setMargin(m_style.metrics.margin);

        m_labelNode->setWidth(labelSize.x);
        m_labelNode->setHeight(labelSize.y);
        m_labelNode->setPosMode(PosMode::relative);
        m_labelNode->setPadding(0.f);
        m_labelNode->setMargin(0.f);
        m_node->addChild(m_labelNode);

        applyCustomLayoutStyle();

        if (state.parentNode != nullptr) {
            state.parentNode->addChild(m_node);
        }

        m_isUIDirty = false;
    }

    void SelectableButtonComp::prepStyle(
        const std::shared_ptr<Core::Style::BessTheme> &theme) {
        UISceneComponent::prepStyle(theme);
        m_selectedTextColor = theme->getColorScheme().getColors().onPrimary;
    }

    bool
    SelectableButtonComp::onMouseButton(const Events::MouseButtonEvent &e) {
        if (e.button != Events::MouseButton::left) {
            return false;
        }

        if (e.action != Events::MouseClickAction::press) {
            return e.action == Events::MouseClickAction::release;
        }

        if (e.details == 0u || e.details == kButtonInfo) {
            activateFromUser();
            return true;
        }

        return false;
    }

    bool SelectableButtonComp::isFocusable() const {
        return true;
    }

    bool SelectableButtonComp::wantsKeyboardInput() const {
        return m_focused;
    }

    bool SelectableButtonComp::onKeyEvent(const SceneEvent &evt) {
        if (evt.type != SceneEvent::Type::key ||
            evt.data.keyPress.action != KeyAction::press) {
            return false;
        }

        if (evt.data.keyPress.keycode == KeyCode::space ||
            evt.data.keyPress.keycode == KeyCode::enter) {
            activateFromUser();
            return true;
        }

        return false;
    }

    void SelectableButtonComp::activateFromUser() {
        if (m_toggleOnClick) {
            m_selected = !m_selected;
        } else {
            m_selected = true;
        }

        if (m_callback) {
            m_callback(m_selected);
        }
    }

    void SelectableButtonComp::drawLabel(SceneDrawContext &state) {
        if (m_labelNode == nullptr || state.renderer == nullptr) {
            return;
        }

        const auto textColor =
            m_selected ? m_selectedTextColor : m_style.textStyle.textColor;
        const auto offsetY = state.renderer->textCenterOffsetY(
            m_name,
            {
                .fontSize = m_style.textStyle.fontSize,
            });
        const auto pos = m_labelNode->getDrawPos();
        const auto drawPos = glm::vec2{
            pos.x - (m_labelNode->getDrawSize().x * 0.5f),
            pos.y + offsetY,
        };

        state.renderer->drawFont(m_name,
                                 {
                                     .position = drawPos,
                                     .fontSize = m_style.textStyle.fontSize,
                                     .color = textColor,
                                     .zIndex = pos.z,
                                     .id =
                                         PickingId{
                                             .runtimeId = resolveRuntimeId(),
                                             .info = kButtonInfo,
                                         },
                                     .transformMode = state.transformMode,
                                 });
    }

    glm::vec2
    SelectableButtonComp::resolveButtonSize(SceneUIPrepareCtx &state) {
        const auto labelSize = state.renderer->measureText(
            getName(),
            {
                .fontSize = m_style.textStyle.fontSize,
            });
        const auto contentSize =
            labelSize + glm::vec2{m_style.metrics.padding.horizontal(),
                                  m_style.metrics.padding.vertical()};

        glm::vec2 size = m_buttonSize;
        if (size.x <= 0.f) {
            size.x = contentSize.x;
        }
        if (size.y <= 0.f) {
            size.y = contentSize.y;
        }

        size.x = std::max({kMinButtonWidth, size.x, contentSize.x});
        size.y = std::max({kMinButtonHeight, size.y, contentSize.y});
        return size;
    }
} // namespace Bess::Canvas::UI
