#include "bess_core/scene/scene_ui/ui_scene_component.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include "common/bess_assert.h"
#include "common/bess_uuid.h"
#include "common/events.h"

namespace Bess::Canvas::UI {
    namespace {
        template <typename T>
        T resolveOptional(const std::optional<T> &custom, const T &defaultVal) {
            return custom.has_value() ? custom.value() : defaultVal;
        }

        void applySizeMode(UINode &node,
                           bool isWidth,
                           LayoutSizeMode mode,
                           float value) {
            switch (mode) {
            case LayoutSizeMode::auto_:
                if (isWidth) {
                    node.setWidthAuto();
                } else {
                    node.setHeightAuto();
                }
                break;
            case LayoutSizeMode::point:
                if (isWidth) {
                    node.setWidth(value);
                } else {
                    node.setHeight(value);
                }
                break;
            case LayoutSizeMode::percent:
                if (isWidth) {
                    node.setWidthPercent(value);
                } else {
                    node.setHeightPercent(value);
                }
                break;
            case LayoutSizeMode::fitContent:
                if (isWidth) {
                    node.setWidthFitContent();
                } else {
                    node.setHeightFitContent();
                }
                break;
            case LayoutSizeMode::maxContent:
                if (isWidth) {
                    node.setWidthMaxContent();
                } else {
                    node.setHeightMaxContent();
                }
                break;
            case LayoutSizeMode::stretch:
                if (isWidth) {
                    node.setWidthStretch();
                } else {
                    node.setHeightStretch();
                }
                break;
            }
        }

        void applyOptionalSize(UINode &node,
                               bool isWidth,
                               const std::optional<LayoutSizeMode> &mode,
                               const std::optional<float> &value) {
            if (!mode.has_value() && !value.has_value()) {
                return;
            }
            applySizeMode(
                node, isWidth, mode.value_or(LayoutSizeMode::point),
                value.value_or(0.f));
        }

        void applyFlexBasis(UINode &node,
                            const std::optional<LayoutSizeMode> &mode,
                            const std::optional<float> &value,
                            const std::optional<Unit> &unit) {
            if (!mode.has_value() && !value.has_value() && !unit.has_value()) {
                return;
            }

            const auto basisValue = value.value_or(0.f);
            if (!mode.has_value()) {
                node.setFlexBasis(basisValue, unit.value_or(Unit::pixel));
                return;
            }

            switch (*mode) {
            case LayoutSizeMode::auto_:
                node.setFlexBasisAuto();
                break;
            case LayoutSizeMode::point:
                node.setFlexBasis(basisValue, Unit::pixel);
                break;
            case LayoutSizeMode::percent:
                node.setFlexBasis(basisValue, Unit::relative);
                break;
            case LayoutSizeMode::fitContent:
                node.setFlexBasisFitContent();
                break;
            case LayoutSizeMode::maxContent:
                node.setFlexBasisMaxContent();
                break;
            case LayoutSizeMode::stretch:
                node.setFlexBasisStretch();
                break;
            }
        }
    } // namespace

    UISceneComponent::UISceneComponent() {
        GAppContext::getInstance()
            .getSubSystem<Bess::EventSystem::EventDispatcher>()
            ->sink<Bess::Events::ThemeChangeEvent>()
            .connect([this](const Bess::Events::ThemeChangeEvent &e) {
                prepStyle(e.theme);
            });
    }

    std::vector<UUID> UISceneComponent::cleanup(SceneState &state,
                                                UUID caller) {
        (void)caller;

        auto reg = state.getUINodeRegistry();

        std::vector<UUID> removedComponents;
        std::vector<UUID> childComponents{
            m_childComponents.begin(),
            m_childComponents.end(),
        };

        for (const auto &childId : childComponents) {
            if (state.isComponentValid(childId)) {
                auto ids = state.removeComponent(childId, m_uuid);
                removedComponents.insert(
                    removedComponents.end(), ids.begin(), ids.end());
            }
        }

        if (m_node != nullptr) {
            reg->removeNode(m_node->getId());
            m_node = nullptr;
        }

        setUIDirty(true);

        return removedComponents;
    }

    void UISceneComponent::prepareUI(SceneUIPrepareCtx &state) {
        initNode(state.sceneState->getUINodeRegistry());

        prepStyle(state.theme);

        const auto size = state.renderer->measureText(
            getName(),
            {
                .fontSize = m_style.textStyle.fontSize,
            });

        m_node->setWidth(size.x);
        m_node->setHeight(size.y);

        m_node->setPadding(m_style.metrics.padding);
        m_node->setMargin(m_style.metrics.margin);
        applyCustomLayoutStyle();

        if (state.parentNode != nullptr) {
            state.parentNode->addChild(m_node);
        }

        m_isUIDirty = false;
    }

    bool UISceneComponent::onMouseEnter(const Events::MouseEnterEvent &e) {
        (void)e;
        m_hovered = true;
        return true;
    }

    bool UISceneComponent::onMouseLeave(const Events::MouseLeaveEvent &e) {
        (void)e;
        m_hovered = false;
        return true;
    }

    void UISceneComponent::onFocusGained(const Events::FocusEvent &e) {
        (void)e;
        m_focused = true;
    }

    void UISceneComponent::onFocusLost(const Events::FocusEvent &e) {
        (void)e;
        m_focused = false;
    }

    Core::Viewport::SceneCursor UISceneComponent::getCursor() const {
        return Core::Viewport::SceneCursor::pointer;
    }

    void UISceneComponent::setIsScreenSpace(bool val) {
        m_transformMode = val ? Core::Renderer::RenderTransformMode::Screen
                              : Core::Renderer::RenderTransformMode::Camera;
    }

    void UISceneComponent::draw(SceneDrawContext &state) {
        onBeforeDraw(state);
        onDraw(state);
        onAfterDraw(state);
    }

    uint32_t UISceneComponent::resolveRuntimeId() const {
        return resolveOptional(m_drawRuntimeId, m_runtimeId);
    }

    void UISceneComponent::onNameChanged() {
        makeUIDirty();
    }

    void
    UISceneComponent::initNode(const std::shared_ptr<UINodeRegistry> &reg) {
        if (m_node == nullptr) {
            m_node = reg->addNode(m_uuid);
            setUINode(m_node);
        }
        m_node->clearChildren();
        m_node->setZVal(m_transform.position.z);
        m_node->setPos(m_transform.position);
        m_node->setPadding(m_style.metrics.padding);
        m_node->setMargin(m_style.metrics.margin);
    }

    void UISceneComponent::makeUIDirty() {
        setUIDirty(true);
        if (m_node != nullptr) {
            m_node->setSizeDirty(true);
            m_node->setPosDirty(true);
        }
    }

    void UISceneComponent::prepStyle(
        const std::shared_ptr<Core::Style::BessTheme> &theme) {
        BESS_ASSERT(theme != nullptr, "Theme must be set in context.");
        m_style = theme->generalElementStyle();

        m_style.metrics.margin =
            resolveOptional(m_customStyle.margin, m_style.metrics.margin);

        m_style.metrics.padding =
            resolveOptional(m_customStyle.padding, m_style.metrics.padding);

        m_style.backgroundColor = resolveOptional(m_customStyle.backgroundColor,
                                                  m_style.backgroundColor);

        m_style.hoverColor =
            resolveOptional(m_customStyle.hoverColor, m_style.hoverColor);

        m_style.borderColor =
            resolveOptional(m_customStyle.borderColor, m_style.borderColor);

        m_style.activeColor =
            resolveOptional(m_customStyle.activeColor, m_style.activeColor);

        m_style.textStyle.fontSize =
            resolveOptional(m_customStyle.fontSize, m_style.textStyle.fontSize);
    }

    void UISceneComponent::applyCustomLayoutStyle() {
        if (m_node == nullptr) {
            return;
        }

        if (m_customStyle.posMode.has_value()) {
            m_node->setPosMode(*m_customStyle.posMode);
        }
        if (m_customStyle.posUnit.has_value()) {
            m_node->setPosUnit(*m_customStyle.posUnit);
        }
        if (m_customStyle.pos.has_value()) {
            m_node->setPos(*m_customStyle.pos);
        }

        applyOptionalSize(
            *m_node, true, m_customStyle.widthMode, m_customStyle.width);
        applyOptionalSize(
            *m_node, false, m_customStyle.heightMode, m_customStyle.height);

        if (m_customStyle.minSize.has_value()) {
            m_node->setMinSize(*m_customStyle.minSize);
        }
        if (m_customStyle.maxSize.has_value()) {
            m_node->setMaxSize(*m_customStyle.maxSize);
        }
        if (m_customStyle.direction.has_value()) {
            m_node->setDirection(*m_customStyle.direction);
        }
        if (m_customStyle.mainAxisAlignment.has_value()) {
            m_node->setMainAxisAlignment(*m_customStyle.mainAxisAlignment);
        }
        if (m_customStyle.crossAxisAlignment.has_value()) {
            m_node->setCrossAxisAlignment(*m_customStyle.crossAxisAlignment);
        }
        if (m_customStyle.alignSelf.has_value()) {
            m_node->setAlignSelf(*m_customStyle.alignSelf);
        }

        if (m_customStyle.flex.has_value()) {
            const auto &flex = *m_customStyle.flex;
            m_node->setFlexGrow(flex.grow);
            m_node->setFlexShrink(flex.shrink);
            m_node->setFlexBasis(flex.basis, flex.basisUnit);
        }
        if (m_customStyle.flexGrow.has_value()) {
            m_node->setFlexGrow(*m_customStyle.flexGrow);
        }
        if (m_customStyle.flexShrink.has_value()) {
            m_node->setFlexShrink(*m_customStyle.flexShrink);
        }
        applyFlexBasis(*m_node,
                       m_customStyle.flexBasisMode,
                       m_customStyle.flexBasis,
                       m_customStyle.flexBasisUnit);

        if (m_customStyle.zVal.has_value()) {
            m_node->setZVal(*m_customStyle.zVal);
        }
        if (m_customStyle.drawPivot.has_value()) {
            m_node->setDrawPivot(*m_customStyle.drawPivot);
        }
    }

    void UISceneComponent::drawBgQuad(SceneDrawContext &state) {
        PickingId pickingId{
            .runtimeId = resolveRuntimeId(),
            .info = 0,
        };

        Core::Renderer::QuadProps quadProps;
        quadProps.position = m_node->getDrawPos();
        quadProps.size = m_node->getDrawSize();
        quadProps.zIndex = m_node->getDrawPos().z;
        quadProps.color =
            m_hovered ? m_style.hoverColor : m_style.backgroundColor;
        quadProps.borderColor = m_style.borderColor;
        quadProps.thickness = m_style.metrics.borderSize.toVec4();
        quadProps.radius = m_style.metrics.borderRadius;
        quadProps.id = pickingId;
        quadProps.transformMode = state.transformMode;

        state.renderer->drawQuad(quadProps);
    }

    void UISceneComponent::drawText(SceneDrawContext &state,
                                    const std::string &text,
                                    UINode *node) {

        SceneComponent *parent = nullptr;

        if (m_parentComponent != UUID::null) {
            parent = state.sceneState->getComponentByUuid(m_parentComponent);
        }

        PickingId pickingId{
            .runtimeId = resolveRuntimeId(),
            .info = 0,
        };

        const auto offsetY = state.renderer->textCenterOffsetY(
            text,
            {
                .fontSize = m_style.textStyle.fontSize,
            });

        auto pos = node->getDrawPos();
        const auto pos_ =
            glm::vec2(pos.x - (node->getDrawSize().x * 0.5f), pos.y + offsetY);

        state.renderer->drawFont(text,
                                 {
                                     .position = pos_,
                                     .fontSize = m_style.textStyle.fontSize,
                                     .color = m_style.textStyle.textColor,
                                     .zIndex = pos.z,
                                     .id = pickingId,
                                     .transformMode = state.transformMode,
                                 });
    }

    void UISceneComponent::onBeforeDraw(SceneDrawContext &state) {
        m_lastTransformMode = state.transformMode;
        // Camera is default so if the component is set to screen space, we
        // override it to screen mode
        if (m_transformMode == Core::Renderer::RenderTransformMode::Screen) {
            state.transformMode = Core::Renderer::RenderTransformMode::Screen;
        }
    }

    void UISceneComponent::onAfterDraw(SceneDrawContext &state) {
        state.transformMode = m_lastTransformMode;
    }

} // namespace Bess::Canvas::UI
