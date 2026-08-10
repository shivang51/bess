#include "bess_core/scene/scene_ui/controls/tree_node_comp.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/scene_event.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include "common/logger.h"
#include <algorithm>

namespace Bess::Canvas::UI {
    namespace {
        constexpr uint32_t kTreeHeaderInfo = 1u;

        [[nodiscard]] bool isToggleKey(const SceneEvent &evt) {
            return evt.type == SceneEvent::Type::key &&
                   evt.data.keyPress.action == KeyAction::press &&
                   (evt.data.keyPress.keycode == KeyCode::space ||
                    evt.data.keyPress.keycode == KeyCode::enter);
        }
    } // namespace

    std::shared_ptr<TreeNodeComp>
    TreeNodeComp::create(const CompConfig &config) {
        return create("", true, nullptr, config);
    }

    std::shared_ptr<TreeNodeComp>
    TreeNodeComp::create(const std::string &label,
                         bool expanded,
                         const UITreeNodeCallback &toggledCallback,
                         const CompConfig &config) {
        auto node = std::make_shared<TreeNodeComp>();
        node->setName(label);
        node->setExpanded(expanded);
        node->setToggledCallback(toggledCallback);
        applyCompConfig(node, config);
        return node;
    }

    void TreeNodeComp::onDraw(SceneDrawContext &state) {
        if (m_headerNode == nullptr || state.renderer == nullptr) {
            return;
        }

        Core::Renderer::QuadProps headerProps;
        headerProps.position = m_headerNode->getDrawPos();
        headerProps.size = m_headerNode->getDrawSize();
        headerProps.zIndex = m_headerNode->getDrawPos().z;
        headerProps.color =
            m_drawHeaderBackground
                ? (m_hovered ? m_style.hoverColor : m_style.backgroundColor)
                : Core::Renderer::Color{0.f, 0.f, 0.f, 0.f};
        headerProps.borderColor =
            m_drawHeaderBorder
                ? (m_focused ? m_style.activeColor : m_style.borderColor)
                : Core::Renderer::Color{0.f, 0.f, 0.f, 0.f};
        headerProps.thickness = m_drawHeaderBorder
                                    ? m_style.metrics.borderSize.toVec4()
                                    : glm::vec4(0.f);
        headerProps.radius = m_style.metrics.borderRadius;
        headerProps.shadow = (m_drawHeaderBackground || m_drawHeaderBorder)
                                 ? m_style.shadowProps
                                 : Core::Renderer::ShadowProps{};
        headerProps.id = {
            .runtimeId = resolveRuntimeId(),
            .info = kTreeHeaderInfo,
        };
        headerProps.transformMode = state.transformMode;
        state.renderer->drawQuad(headerProps);

        drawChevron(state);
        drawText(state, m_name, m_labelNode);
        drawChildren(state);
    }

    void TreeNodeComp::prepareUI(SceneUIPrepareCtx &state) {
        prepStyle(state.theme);
        initNode(state.sceneState->getUINodeRegistry());

        if (m_headerNode == nullptr) {
            m_headerNode =
                state.sceneState->getUINodeRegistry()->addNode(UUID());
        }
        if (m_chevronNode == nullptr) {
            m_chevronNode =
                state.sceneState->getUINodeRegistry()->addNode(UUID());
        }
        if (m_labelNode == nullptr) {
            m_labelNode =
                state.sceneState->getUINodeRegistry()->addNode(UUID());
        }
        if (m_contentNode == nullptr) {
            m_contentNode =
                state.sceneState->getUINodeRegistry()->addNode(UUID());
        }

        const auto labelSize = state.renderer->measureText(
            m_name,
            {
                .fontSize = m_style.textStyle.fontSize,
            });
        const auto chevronSize = std::max(1.f, m_chevronSize);
        const auto verticalPadding = m_style.metrics.padding.vertical();
        const auto headerHeight =
            std::max(labelSize.y, chevronSize) + verticalPadding;

        m_node->setDirection(LayoutDirection::vertical);
        m_node->setWidthFitContent();
        m_node->setHeightFitContent();
        m_node->setCrossAxisAlignment(LayoutAlignment::start);
        m_node->setPadding(0.f);
        m_node->setMargin(m_style.metrics.margin);

        m_headerNode->setDirection(LayoutDirection::horizontal);
        m_headerNode->setWidthFitContent();
        m_headerNode->setHeight(headerHeight);
        m_headerNode->setCrossAxisAlignment(LayoutAlignment::center);
        auto headerPadding = m_style.metrics.padding;
        headerPadding.left += std::max(0.f, m_headerContentIndent);
        m_headerNode->setPadding(headerPadding);
        m_headerNode->setMargin(0.f);
        m_node->addChild(m_headerNode);

        m_chevronNode->setWidth(chevronSize);
        m_chevronNode->setHeight(chevronSize);
        m_chevronNode->setPosMode(PosMode::relative);
        m_chevronNode->setPadding(0.f);
        m_chevronNode->setMargin(Core::Style::Margin::onlyRight(
            std::max(0.f, m_chevronLabelSpacing)));
        m_headerNode->addChild(m_chevronNode);

        m_labelNode->setWidth(labelSize.x);
        m_labelNode->setHeight(labelSize.y);
        m_labelNode->setPosMode(PosMode::relative);
        m_labelNode->setPadding(0.f);
        m_labelNode->setMargin(0.f);
        m_headerNode->addChild(m_labelNode);

        if (m_expanded) {
            const float childIndent = headerPadding.left + chevronSize +
                                      std::max(0.f, m_chevronLabelSpacing) +
                                      std::max(0.f, m_contentIndent);
            m_contentNode->setDirection(LayoutDirection::vertical);
            m_contentNode->setWidthFitContent();
            m_contentNode->setHeightFitContent();
            m_contentNode->setCrossAxisAlignment(LayoutAlignment::start);
            m_contentNode->setPadding(
                Core::Style::Padding::onlyLeft(childIndent));
            m_contentNode->setMargin(0.f);
            m_node->addChild(m_contentNode);
            prepChildren(state);
        }

        applyCustomLayoutStyle();

        if (state.parentNode != nullptr) {
            state.parentNode->addChild(m_node);
        }

        m_isUIDirty = false;
    }

    bool TreeNodeComp::onMouseButton(const Events::MouseButtonEvent &e) {
        if (e.button != Events::MouseButton::left ||
            (e.details != 0u && e.details != kTreeHeaderInfo)) {
            return false;
        }

        if (e.action == Events::MouseClickAction::press) {
            toggleFromUser();
        }

        return e.action == Events::MouseClickAction::press ||
               e.action == Events::MouseClickAction::release;
    }

    bool TreeNodeComp::isFocusable() const {
        return true;
    }

    bool TreeNodeComp::wantsKeyboardInput() const {
        return m_focused;
    }

    bool TreeNodeComp::onKeyEvent(const SceneEvent &evt) {
        if (!isToggleKey(evt)) {
            return false;
        }

        toggleFromUser();
        return true;
    }

    void TreeNodeComp::toggleFromUser() {
        m_expanded = !m_expanded;
        makeUIDirty();
        if (m_toggledCallback) {
            m_toggledCallback(m_expanded);
        }
    }

    void TreeNodeComp::prepChildren(SceneUIPrepareCtx &state) {
        auto prevParent = state.parentNode;
        state.parentNode = m_contentNode;
        for (const auto &childId : m_childComponents) {
            auto childComp = state.sceneState->getComponentByUuid(childId);
            if (childComp == nullptr) {
                BESS_WARN("Tree child component with UUID {} not found.",
                          (uint64_t)childId);
                continue;
            }
            childComp->prepareUI(state);
        }
        state.parentNode = prevParent;
    }

    void TreeNodeComp::drawChildren(SceneDrawContext &state) {
        if (!m_expanded) {
            return;
        }

        for (const auto &childId : m_childComponents) {
            auto childComp = state.sceneState->getComponentByUuid(childId);
            if (childComp != nullptr) {
                childComp->draw(state);
            }
        }
    }

    void TreeNodeComp::drawChevron(SceneDrawContext &state) {
        if (m_chevronNode == nullptr || state.renderer == nullptr) {
            return;
        }

        const auto center = m_chevronNode->getDrawPos();
        const auto size = m_chevronNode->getDrawSize();
        const float half = std::min(size.x, size.y) * 0.28f;
        const float thickness = std::max(1.f, std::min(size.x, size.y) * 0.14f);
        const PickingId id{
            .runtimeId = resolveRuntimeId(),
            .info = kTreeHeaderInfo,
        };

        Core::Renderer::PathProps path;
        path.strokeColor = m_style.textStyle.textColor;
        path.strokeSize = thickness;
        path.zIndex = center.z + 0.0001f;
        path.id = id;
        path.closePath = false;
        path.lineJoin = Core::Renderer::PathLineJoin::Round;
        path.lineCap = Core::Renderer::PathLineCap::Round;
        path.transformMode = state.transformMode;

        state.renderer->beginPath(path);
        if (m_expanded) {
            state.renderer->pathMoveTo(
                {center.x - half, center.y - (half * 0.45f)});
            state.renderer->pathLineTo(
                {center.x, center.y + (half * 0.55f)}, thickness, id);
            state.renderer->pathLineTo(
                {center.x + half, center.y - (half * 0.45f)}, thickness, id);
            state.renderer->endPath();
            return;
        }

        state.renderer->pathMoveTo(
            {center.x - (half * 0.45f), center.y - half});
        state.renderer->pathLineTo(
            {center.x + (half * 0.55f), center.y}, thickness, id);
        state.renderer->pathLineTo(
            {center.x - (half * 0.45f), center.y + half}, thickness, id);
        state.renderer->endPath();
    }
} // namespace Bess::Canvas::UI
