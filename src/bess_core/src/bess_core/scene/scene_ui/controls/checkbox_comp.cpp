#include "bess_core/scene/scene_ui/controls/checkbox_comp.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/scene_event.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include <algorithm>

namespace Bess::Canvas::UI {
    namespace {
        constexpr uint32_t kCheckboxLabelInfo = 0u;
        constexpr uint32_t kCheckboxBoxInfo = 1u;

        [[nodiscard]] bool isKeyboardToggle(const SceneEvent &evt) {
            if (evt.type != SceneEvent::Type::key ||
                evt.data.keyPress.action != KeyAction::press) {
                return false;
            }

            return evt.data.keyPress.keycode == KeyCode::space ||
                   evt.data.keyPress.keycode == KeyCode::enter;
        }
    } // namespace

    std::shared_ptr<CheckboxComp>
    CheckboxComp::create(const std::string &label,
                         const UICheckboxCallback &callback,
                         bool checked) {
        auto checkbox = std::make_shared<CheckboxComp>();
        checkbox->setName(label);
        checkbox->setCallback(callback);
        checkbox->setChecked(checked);
        return checkbox;
    }

    void CheckboxComp::draw(SceneDrawContext &state) {
        if (m_boxNode == nullptr || state.renderer == nullptr) {
            return;
        }

        if (m_showLabel && m_labelNode != nullptr) {
            drawText(state, m_name, m_labelNode);
        }

        const PickingId boxId{
            .runtimeId = resolveRuntimeId(),
            .info = kCheckboxBoxInfo,
        };

        Core::Renderer::QuadProps boxProps;
        boxProps.position = m_boxNode->getDrawPos();
        boxProps.size = m_boxNode->getDrawSize();
        boxProps.zIndex = m_boxNode->getDrawPos().z;
        boxProps.color = m_checked
                             ? m_checkedColor
                             : (m_hovered ? m_style.hoverColor : m_boxColor);
        boxProps.borderColor = m_checked ? m_checkedColor : m_style.borderColor;
        boxProps.thickness = m_style.metrics.borderSize.toVec4();
        boxProps.radius = m_style.metrics.borderRadius;
        boxProps.id = boxId;
        boxProps.transformMode = state.transformMode;
        state.renderer->drawQuad(boxProps);

        if (!m_checked) {
            return;
        }

        const auto center = m_boxNode->getDrawPos();
        const auto size = m_boxNode->getDrawSize();
        const float thickness = std::max(1.2f, size.y * 0.14f);
        const float z = center.z + 0.0001f;

        Core::Renderer::PathProps checkProps;
        checkProps.strokeColor = m_checkColor;
        checkProps.strokeSize = thickness;
        checkProps.zIndex = z;
        checkProps.id = boxId;
        checkProps.closePath = false;
        checkProps.lineJoin = Core::Renderer::PathLineJoin::Round;
        checkProps.lineCap = Core::Renderer::PathLineCap::Round;
        checkProps.transformMode = state.transformMode;

        state.renderer->beginPath(checkProps);
        state.renderer->pathMoveTo(
            {center.x - (size.x * 0.30f), center.y + (size.y * 0.02f)});
        state.renderer->pathLineTo(
            {center.x - (size.x * 0.08f), center.y + (size.y * 0.23f)},
            thickness,
            boxId);
        state.renderer->pathLineTo(
            {center.x + (size.x * 0.34f), center.y - (size.y * 0.28f)},
            thickness,
            boxId);
        state.renderer->endPath();
    }

    void CheckboxComp::prepareUI(SceneUIPrepareCtx &state) {
        prepStyle(state.theme);
        initNode(state.sceneState->getUINodeRegistry());

        if (m_boxNode == nullptr) {
            m_boxNode = state.sceneState->getUINodeRegistry()->addNode(UUID());
        }
        if (m_labelNode == nullptr) {
            m_labelNode =
                state.sceneState->getUINodeRegistry()->addNode(UUID());
        }

        const auto &colors = state.theme->getColorScheme().getColors();
        m_boxColor = colors.surfaceContainerLow;
        m_checkedColor = colors.primary;
        m_checkColor = colors.onPrimary;

        m_node->setDirection(LayoutDirection::horizontal);
        m_node->setWidthFitContent();
        m_node->setHeightFitContent();
        m_node->setCrossAxisAlignment(LayoutAlignment::center);
        m_node->setPadding(m_style.metrics.padding);
        m_node->setMargin(m_style.metrics.margin);

        m_boxNode->setWidth(std::max(1.f, m_boxSize.x));
        m_boxNode->setHeight(std::max(1.f, m_boxSize.y));
        m_boxNode->setPosMode(PosMode::relative);
        m_boxNode->setPadding(0.f);
        m_boxNode->setMargin(m_showLabel ? Core::Style::Margin::onlyRight(
                                               std::max(0.f, m_labelBoxSpacing))
                                         : Core::Style::Margin(0.f));
        m_node->addChild(m_boxNode);

        if (m_showLabel) {
            const auto labelSize = state.renderer->measureText(
                m_name,
                {
                    .fontSize = m_style.textStyle.fontSize,
                });

            m_labelNode->setWidth(labelSize.x);
            m_labelNode->setHeight(labelSize.y);
            m_labelNode->setPosMode(PosMode::relative);
            m_labelNode->setPadding(0.f);
            m_labelNode->setMargin(0.f);
            m_node->addChild(m_labelNode);
        }

        if (state.parentNode != nullptr) {
            state.parentNode->addChild(m_node);
        }

        m_isUIDirty = false;
    }

    bool CheckboxComp::onMouseButton(const Events::MouseButtonEvent &e) {
        if (e.button != Events::MouseButton::left) {
            return false;
        }

        const bool interactivePart =
            e.details == kCheckboxLabelInfo || e.details == kCheckboxBoxInfo;
        if (!interactivePart) {
            return false;
        }

        if (e.action == Events::MouseClickAction::press) {
            toggleFromUser();
        }

        return e.action == Events::MouseClickAction::press ||
               e.action == Events::MouseClickAction::release;
    }

    bool CheckboxComp::isFocusable() const {
        return true;
    }

    bool CheckboxComp::wantsKeyboardInput() const {
        return m_focused;
    }

    bool CheckboxComp::onKeyEvent(const SceneEvent &evt) {
        if (!isKeyboardToggle(evt)) {
            return false;
        }

        toggleFromUser();
        return true;
    }

    void CheckboxComp::toggleFromUser() {
        m_checked = !m_checked;
        if (m_callback) {
            m_callback(m_checked);
        }
    }
} // namespace Bess::Canvas::UI
