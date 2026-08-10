#include "bess_core/scene/scene_ui/controls/panel_comp.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/camera.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include "common/logger.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Bess::Canvas::UI {
    namespace {
        constexpr uint32_t kPanelInfo = 1u;
        constexpr uint32_t kHeaderInfo = 2u;
        constexpr uint32_t kResizeInfoFlag = 1u << 8u;
        constexpr uint32_t kResizeInfoMask = 0x0fu;
        constexpr float kMinHeaderHeight = 16.f;
        constexpr float kMinResizeGripSize = 6.f;
        constexpr float kSeparatorHeight = 1.f;
        constexpr float kMinInnerPanelWidth = 32.f;
        constexpr float kHeaderPaddingX = 8.f;
        constexpr float kHeaderPaddingY = 2.f;

        [[nodiscard]] float finiteOr(float value, float fallback) {
            return std::isfinite(value) ? value : fallback;
        }

        [[nodiscard]] float edge(float value) {
            return std::max(0.f, finiteOr(value, 0.f));
        }

        [[nodiscard]] bool noResizeEdges(UIPanelResizeEdge edges) {
            return resizeEdgeMask(edges) == 0u;
        }

        [[nodiscard]] glm::vec2
        clipToPixels(const glm::vec4 &clip, float width, float height) {
            const float invW = clip.w != 0.f ? 1.f / clip.w : 1.f;
            const float ndcX = clip.x * invW;
            const float ndcY = clip.y * invW;
            return {
                ((ndcX * 0.5f) + 0.5f) * width,
                (1.f - ((ndcY * 0.5f) + 0.5f)) * height,
            };
        }
    } // namespace

    std::shared_ptr<PanelComp> PanelComp::create(const CompConfig &config) {
        return create("", config);
    }

    std::shared_ptr<PanelComp> PanelComp::create(const std::string &title,
                                                 const CompConfig &config) {
        auto panel = std::make_shared<PanelComp>();
        panel->setName(title);
        applyCompConfig(panel, config);
        return panel;
    }

    std::shared_ptr<PanelComp> PanelComp::create(const std::string &title,
                                                 const glm::vec2 &panelSize,
                                                 const CompConfig &config) {
        auto panel = std::make_shared<PanelComp>();
        panel->setName(title);
        panel->setPanelSize(panelSize);
        applyCompConfig(panel, config);
        return panel;
    }

    UIPanelResizeEdge PanelComp::getResizeEdges() const {
        return m_resizeEdges;
    }

    void PanelComp::setResizeEdges(UIPanelResizeEdge edges) {
        m_resizeEdges = edges;
        makeUIDirty();
    }

    bool PanelComp::isResizeEdgeEnabled(UIPanelResizeEdge edge) const {
        return hasResizeEdge(m_resizeEdges, edge);
    }

    void PanelComp::setResizeEdgeEnabled(UIPanelResizeEdge edge, bool enabled) {
        const uint8_t edgeMask = resizeEdgeMask(edge);
        uint8_t mask = resizeEdgeMask(m_resizeEdges);
        mask = enabled ? static_cast<uint8_t>(mask | edgeMask)
                       : static_cast<uint8_t>(mask & ~edgeMask);
        setResizeEdges(static_cast<UIPanelResizeEdge>(mask));
    }

    std::vector<UUID> PanelComp::cleanup(SceneState &state, UUID caller) {
        auto removedComponents = UISceneComponent::cleanup(state, caller);
        auto registry = state.getUINodeRegistry();
        if (registry != nullptr) {
            if (m_headerNode != nullptr) {
                registry->removeNode(m_headerNode->getId());
                m_headerNode = nullptr;
            }
            if (m_titleNode != nullptr) {
                registry->removeNode(m_titleNode->getId());
                m_titleNode = nullptr;
            }
            if (m_contentNode != nullptr) {
                registry->removeNode(m_contentNode->getId());
                m_contentNode = nullptr;
            }
            if (m_resizeGripNode != nullptr) {
                registry->removeNode(m_resizeGripNode->getId());
                m_resizeGripNode = nullptr;
            }
        }
        return removedComponents;
    }

    void PanelComp::onDraw(SceneDrawContext &state) {
        if (m_node == nullptr || state.renderer == nullptr) {
            return;
        }

        drawPanelBackground(state);
        drawHeader(state);
        drawChildren(state);
        drawResizeGrip(state);
        drawResizeHitRegions(state);
    }

    void PanelComp::prepareUI(SceneUIPrepareCtx &state) {
        prepStyle(state.theme);
        const auto registry = state.sceneState->getUINodeRegistry();
        initNode(registry);
        ensureNodes(registry);

        const auto titleSize = state.renderer->measureText(
            m_name,
            {
                .fontSize = m_style.textStyle.fontSize,
            });
        const float headerHeight = resolveHeaderHeight(titleSize);
        m_cachedHeaderHeight = headerHeight;
        m_panelSize = clampPanelSize(m_panelSize);

        configurePanelNode();
        configureHeaderNode(titleSize, headerHeight);
        configureContentNode(headerHeight);
        configureResizeGripNode();

        m_node->addChild(m_headerNode);
        m_node->addChild(m_contentNode);
        if (m_resizable && m_drawResizeGrip) {
            m_node->addChild(m_resizeGripNode);
        }

        prepareChildren(state);

        if (state.parentNode != nullptr) {
            state.parentNode->addChild(m_node);
        }

        m_isUIDirty = false;
    }

    void
    PanelComp::prepStyle(const std::shared_ptr<Core::Style::BessTheme> &theme) {
        UISceneComponent::prepStyle(theme);

        const auto &colors = theme->getColorScheme().getColors();
        if (m_customStyle.width.has_value() &&
            m_customStyle.widthMode.value_or(LayoutSizeMode::point) ==
                LayoutSizeMode::point) {
            m_panelSize.x = std::max(0.f, *m_customStyle.width);
        }
        if (m_customStyle.height.has_value() &&
            m_customStyle.heightMode.value_or(LayoutSizeMode::point) ==
                LayoutSizeMode::point) {
            m_panelSize.y = std::max(0.f, *m_customStyle.height);
        }
        if (m_customStyle.minSize.has_value()) {
            m_minPanelSize = *m_customStyle.minSize;
        }
        if (m_customStyle.maxSize.has_value()) {
            m_maxPanelSize = *m_customStyle.maxSize;
        }
        if (m_customStyle.drawBg.has_value()) {
            m_drawBackground = *m_customStyle.drawBg;
        }

        m_headerColor = m_style.hoverColor;
        m_headerHoverColor = m_customStyle.hoverColor.has_value()
                                 ? m_style.hoverColor
                                 : colors.surfaceContainerHighest;
        m_separatorColor = m_style.borderColor.withAlpha(0.72f);
        m_resizeGripColor = m_style.borderColor.withAlpha(0.86f);
    }

    bool PanelComp::onMouseEnter(const Events::MouseEnterEvent &e) {
        UISceneComponent::onMouseEnter(e);
        m_hoveredInfo = e.details;
        return true;
    }

    bool PanelComp::onMouseLeave(const Events::MouseLeaveEvent &e) {
        UISceneComponent::onMouseLeave(e);
        if (!m_resizing) {
            m_hoveredInfo = 0u;
        }
        return true;
    }

    bool PanelComp::onMouseButton(const Events::MouseButtonEvent &e) {
        if (e.button != Events::MouseButton::left) {
            return false;
        }

        if (e.action == Events::MouseClickAction::release) {
            if (m_resizing) {
                updateSizeFromPointer(e.mousePos);
                m_resizing = false;
                m_activeResizeEdges = UIPanelResizeEdge::none;
                return true;
            }

            return e.details == kPanelInfo || e.details == kHeaderInfo ||
                   isResizeInfo(e.details);
        }

        if (e.action != Events::MouseClickAction::press) {
            return false;
        }

        const auto edges = resizeEdgesFromInfo(e.details);
        if (canResizeFromEdges(edges)) {
            m_resizing = true;
            m_activeResizeEdges = edges;
            m_resizeStartPointer = e.mousePos;
            m_resizeStartSize = m_panelSize;
            m_resizeStartPosition = m_transform.position;
            return true;
        }

        return e.details == kPanelInfo || e.details == kHeaderInfo;
    }

    bool PanelComp::onPointerMove(const Events::MouseMoveEvent &e) {
        if (!m_resizing) {
            return false;
        }

        updateSizeFromPointer(e.mousePos);
        return true;
    }

    bool PanelComp::hasPointerCapture() const {
        return m_resizing;
    }

    bool PanelComp::isFocusable() const {
        return true;
    }

    Core::Viewport::SceneCursor PanelComp::getCursor() const {
        if (m_resizing) {
            return cursorForResizeEdges(m_activeResizeEdges);
        }
        if (m_resizable) {
            return cursorForResizeEdges(hoveredResizeEdges());
        }
        return Core::Viewport::SceneCursor::normal;
    }

    void
    PanelComp::ensureNodes(const std::shared_ptr<UINodeRegistry> &registry) {
        if (m_headerNode == nullptr) {
            m_headerNode = registry->addNode(UUID());
        }
        if (m_titleNode == nullptr) {
            m_titleNode = registry->addNode(UUID());
        }
        if (m_contentNode == nullptr) {
            m_contentNode = registry->addNode(UUID());
        }
        if (m_resizeGripNode == nullptr) {
            m_resizeGripNode = registry->addNode(UUID());
        }
    }

    void PanelComp::configurePanelNode() {
        const glm::vec2 minSize = resolvedMinPanelSize();
        const glm::vec2 maxSize = resolvedMaxPanelSize(minSize);

        m_node->setDirection(LayoutDirection::vertical);
        m_node->setMainAxisAlignment(LayoutAlignment::start);
        m_node->setCrossAxisAlignment(LayoutAlignment::start);
        if (!m_customStyle.drawPivot.has_value()) {
            m_node->setDrawPivot(DrawPivot::topLeft);
        }
        m_node->setWidth(m_panelSize.x);
        m_node->setHeight(m_panelSize.y);
        m_node->setMinSize(minSize);
        m_node->setMaxSize(maxSize);
        m_node->setPadding(Core::Style::Padding::zero());
        m_node->setMargin(m_style.metrics.margin);

        applyCustomLayoutStyle();

        m_node->setDirection(LayoutDirection::vertical);
        m_node->setMainAxisAlignment(LayoutAlignment::start);
        m_node->setCrossAxisAlignment(LayoutAlignment::start);
        m_node->setWidth(m_panelSize.x);
        m_node->setHeight(m_panelSize.y);
        m_node->setMinSize(minSize);
        m_node->setMaxSize(maxSize);
        m_node->setPadding(Core::Style::Padding::zero());
        m_node->setMargin(m_style.metrics.margin);
    }

    void PanelComp::configureHeaderNode(const glm::vec2 &titleSize,
                                        float headerHeight) {
        const auto border = resolvedBorderSize();
        const float innerWidth =
            std::max(0.f, m_panelSize.x - border.left - border.right);
        const float innerHeight =
            std::max(0.f, m_panelSize.y - border.top - border.bottom);
        const float resolvedHeight = std::min(headerHeight, innerHeight);
        const auto padding = headerPadding();

        m_headerNode->clearChildren();
        m_headerNode->setDirection(LayoutDirection::horizontal);
        m_headerNode->setMainAxisAlignment(LayoutAlignment::start);
        m_headerNode->setCrossAxisAlignment(LayoutAlignment::center);
        m_headerNode->setWidth(innerWidth);
        m_headerNode->setHeight(resolvedHeight);
        m_headerNode->setPadding(padding);
        m_headerNode->setMargin(0.f);
        m_headerNode->setPosMode(PosMode::absolute);
        m_headerNode->setPos({border.left, border.top});
        m_headerNode->setZVal(0.0001f);

        const float availableTitleWidth =
            std::max(0.f, innerWidth - padding.horizontal());
        m_titleNode->setWidth(std::min(titleSize.x, availableTitleWidth));
        m_titleNode->setHeight(titleSize.y);
        m_titleNode->setPadding(0.f);
        m_titleNode->setMargin(0.f);
        m_titleNode->setPosMode(PosMode::relative);
        m_titleNode->setZVal(0.0001f);
        m_headerNode->addChild(m_titleNode);
    }

    void PanelComp::configureContentNode(float headerHeight) {
        const auto border = resolvedBorderSize();
        const float innerWidth =
            std::max(0.f, m_panelSize.x - border.left - border.right);
        const float innerHeight =
            std::max(0.f, m_panelSize.y - border.top - border.bottom);
        const float resolvedHeaderHeight = std::min(headerHeight, innerHeight);

        m_contentNode->clearChildren();
        m_contentNode->setDirection(LayoutDirection::vertical);
        m_contentNode->setMainAxisAlignment(LayoutAlignment::start);
        m_contentNode->setCrossAxisAlignment(m_contentAlignment);
        m_contentNode->setWidth(innerWidth);
        m_contentNode->setHeight(
            std::max(0.f, innerHeight - resolvedHeaderHeight));
        m_contentNode->setPadding(m_style.metrics.padding);
        m_contentNode->setMargin(0.f);
        m_contentNode->setPosMode(PosMode::absolute);
        m_contentNode->setPos({border.left, border.top + resolvedHeaderHeight});
        m_contentNode->setZVal(0.0001f);
    }

    void PanelComp::configureResizeGripNode() {
        const auto border = resolvedBorderSize();
        const float innerWidth =
            std::max(0.f, m_panelSize.x - border.left - border.right);
        const float innerHeight =
            std::max(0.f, m_panelSize.y - border.top - border.bottom);
        const float gripSize =
            std::max(kMinResizeGripSize, finiteOr(m_resizeGripSize, 0.f));
        m_resizeGripNode->setWidth(gripSize);
        m_resizeGripNode->setHeight(gripSize);
        m_resizeGripNode->setPadding(0.f);
        m_resizeGripNode->setMargin(0.f);
        m_resizeGripNode->setPosMode(PosMode::absolute);
        m_resizeGripNode->setPos({
            border.left + std::max(0.f, innerWidth - gripSize),
            border.top + std::max(0.f, innerHeight - gripSize),
        });
        m_resizeGripNode->setZVal(0.0004f);
    }

    void PanelComp::prepareChildren(SceneUIPrepareCtx &state) {
        auto *previousParent = state.parentNode;
        state.parentNode = m_contentNode;
        for (const auto &childId : m_childComponents) {
            auto *childComp = state.sceneState->getComponentByUuid(childId);
            if (childComp == nullptr) {
                BESS_WARN("Panel child component with UUID {} not found.",
                          static_cast<uint64_t>(childId));
                continue;
            }
            childComp->prepareUI(state);
        }
        state.parentNode = previousParent;
    }

    void PanelComp::drawPanelBackground(SceneDrawContext &state) {
        if (!m_drawBackground) {
            return;
        }

        const PickingId id{
            .runtimeId = resolveRuntimeId(),
            .info = kPanelInfo,
        };

        Core::Renderer::QuadProps props;
        const auto border = resolvedBorderSize();
        props.position = m_node->getDrawPos();
        props.size = m_node->getDrawSize();
        props.zIndex = m_node->getDrawPos().z;
        props.color = m_style.backgroundColor;
        props.borderColor =
            m_focused ? m_style.activeColor : m_style.borderColor;
        props.thickness = border.toVec4();
        props.radius = m_style.metrics.borderRadius;
        props.shadow = m_style.shadowProps;
        props.id = id;
        props.transformMode = state.transformMode;
        state.renderer->drawQuad(props);
    }

    void PanelComp::drawHeader(SceneDrawContext &state) {
        if (m_headerNode == nullptr || m_titleNode == nullptr) {
            return;
        }

        const PickingId id{
            .runtimeId = resolveRuntimeId(),
            .info = kHeaderInfo,
        };

        Core::Renderer::QuadProps headerProps;
        const auto border = resolvedBorderSize();
        headerProps.position = m_headerNode->getDrawPos();
        headerProps.size = m_headerNode->getDrawSize();
        headerProps.zIndex = m_headerNode->getDrawPos().z;
        headerProps.color =
            m_hoveredInfo == kHeaderInfo ? m_headerHoverColor : m_headerColor;
        headerProps.radius = {
            std::max(0.f, m_style.metrics.borderRadius.x - border.left),
            std::max(0.f, m_style.metrics.borderRadius.y - border.right),
            0.f,
            0.f,
        };
        headerProps.id = id;
        headerProps.transformMode = state.transformMode;
        state.renderer->drawQuad(headerProps);

        const auto headerRect = nodeRect(m_headerNode);
        Core::Renderer::QuadProps separatorProps;
        separatorProps.position = {
            headerProps.position.x,
            headerRect.bottom - (kSeparatorHeight * 0.5f),
        };
        separatorProps.size = {headerProps.size.x, kSeparatorHeight};
        separatorProps.zIndex = headerProps.zIndex + 0.0001f;
        separatorProps.color = m_separatorColor;
        separatorProps.id = id;
        separatorProps.transformMode = state.transformMode;
        state.renderer->drawQuad(separatorProps);

        const Rect titleRect =
            intersectRect(nodeRect(m_titleNode), innerPanelRect());
        const bool clipped = pushClip(state, titleRect);
        const bool canDrawTitle =
            !rectEmpty(titleRect) && (state.camera == nullptr || clipped);
        if (canDrawTitle) {
            drawText(state, m_name, m_titleNode, id);
        }
        if (clipped) {
            state.renderer->popScissorRect();
        }
    }

    void PanelComp::drawChildren(SceneDrawContext &state) {
        if (m_contentNode == nullptr) {
            return;
        }

        const Rect contentRect =
            intersectRect(nodeRect(m_contentNode), innerPanelRect());
        if (rectEmpty(contentRect)) {
            return;
        }

        const bool clipped = pushClip(state, contentRect);
        if (state.camera != nullptr && !clipped) {
            return;
        }

        for (const auto &childId : m_childComponents) {
            auto childComp = state.sceneState->getComponentByUuid(childId);
            if (childComp != nullptr) {
                childComp->draw(state);
            }
        }

        if (clipped) {
            state.renderer->popScissorRect();
        }
    }

    void PanelComp::drawResizeGrip(SceneDrawContext &state) {
        const auto edges = UIPanelResizeEdge::right | UIPanelResizeEdge::bottom;
        if (!m_drawResizeGrip || m_resizeGripNode == nullptr ||
            !canResizeFromEdges(edges)) {
            return;
        }

        const PickingId id{.runtimeId = resolveRuntimeId(),
                           .info = resizeInfo(edges)};

        const auto pos = m_resizeGripNode->getDrawPos();
        const auto size = m_resizeGripNode->getDrawSize();
        const float z = pos.z + 0.0002f;

        Core::Renderer::QuadProps hitArea;
        hitArea.position = pos;
        hitArea.size = size;
        hitArea.zIndex = z;
        hitArea.color = Core::Renderer::Color{0.f, 0.f, 0.f, 0.f};
        hitArea.id = id;
        hitArea.transformMode = state.transformMode;
        state.renderer->drawQuad(hitArea);

        const float right = pos.x + (size.x * 0.5f);
        const float bottom = pos.y + (size.y * 0.5f);
        const float inset = std::max(2.f, size.x * 0.18f);
        const float gap = std::max(3.f, size.x * 0.24f);
        const float thickness = std::max(1.f, size.x * 0.08f);
        const auto color =
            (m_resizing || resizeEdgesFromInfo(m_hoveredInfo) == edges)
                ? m_style.activeColor
                : m_resizeGripColor;

        for (int i = 0; i < 3; ++i) {
            const float offset = inset + (static_cast<float>(i) * gap);
            Core::Renderer::LineProps line;
            line.p0 = {right - offset, bottom - inset};
            line.p1 = {right - inset, bottom - offset};
            line.thickness = thickness;
            line.zIndex = z + 0.0001f;
            line.color = color;
            line.id = id;
            line.transformMode = state.transformMode;
            state.renderer->drawLine(line);
        }
    }

    void PanelComp::drawResizeHitRegions(SceneDrawContext &state) {
        if (!m_resizable || state.renderer == nullptr ||
            noResizeEdges(m_resizeEdges) || m_node == nullptr) {
            return;
        }

        const float z = m_node->getDrawPos().z + 0.001f;

        drawResizeHitRegion(state, UIPanelResizeEdge::top, z);
        drawResizeHitRegion(state, UIPanelResizeEdge::right, z);
        drawResizeHitRegion(state, UIPanelResizeEdge::bottom, z);
        drawResizeHitRegion(state, UIPanelResizeEdge::left, z);

        drawResizeHitRegion(state,
                            UIPanelResizeEdge::top | UIPanelResizeEdge::left,
                            z + 0.0001f);
        drawResizeHitRegion(state,
                            UIPanelResizeEdge::top | UIPanelResizeEdge::right,
                            z + 0.0001f);
        drawResizeHitRegion(state,
                            UIPanelResizeEdge::bottom |
                                UIPanelResizeEdge::right,
                            z + 0.0001f);
        drawResizeHitRegion(state,
                            UIPanelResizeEdge::bottom | UIPanelResizeEdge::left,
                            z + 0.0001f);
    }

    bool PanelComp::canResizeFromEdges(UIPanelResizeEdge edges) const {
        const uint8_t mask = resizeEdgeMask(edges);
        return m_resizable && mask != 0u &&
               (mask & ~resizeEdgeMask(m_resizeEdges)) == 0u;
    }

    bool PanelComp::isResizeInfo(uint32_t info) const {
        return (info & kResizeInfoFlag) != 0u && (info & kResizeInfoMask) != 0u;
    }

    uint32_t PanelComp::resizeInfo(UIPanelResizeEdge edges) const {
        return kResizeInfoFlag | resizeEdgeMask(edges);
    }

    UIPanelResizeEdge PanelComp::resizeEdgesFromInfo(uint32_t info) const {
        if (!isResizeInfo(info)) {
            return UIPanelResizeEdge::none;
        }

        return static_cast<UIPanelResizeEdge>(info & kResizeInfoMask);
    }

    UIPanelResizeEdge PanelComp::hoveredResizeEdges() const {
        return resizeEdgesFromInfo(m_hoveredInfo);
    }

    Core::Viewport::SceneCursor
    PanelComp::cursorForResizeEdges(UIPanelResizeEdge edges) const {
        if (!canResizeFromEdges(edges)) {
            return Core::Viewport::SceneCursor::normal;
        }

        const bool left = hasResizeEdge(edges, UIPanelResizeEdge::left);
        const bool right = hasResizeEdge(edges, UIPanelResizeEdge::right);
        const bool top = hasResizeEdge(edges, UIPanelResizeEdge::top);
        const bool bottom = hasResizeEdge(edges, UIPanelResizeEdge::bottom);

        if ((left && top) || (right && bottom)) {
            return Core::Viewport::SceneCursor::resizeDiagonalNWSE;
        }
        if ((right && top) || (left && bottom)) {
            return Core::Viewport::SceneCursor::resizeDiagonalNESW;
        }
        if (left || right) {
            return Core::Viewport::SceneCursor::resizeHorizontal;
        }
        if (top || bottom) {
            return Core::Viewport::SceneCursor::resizeVertical;
        }

        return Core::Viewport::SceneCursor::normal;
    }

    PanelComp::Rect PanelComp::resizeHitRect(UIPanelResizeEdge edges) const {
        Rect rect = nodeRect(m_node);
        const auto border = resolvedBorderSize();
        const float hitSize = std::max({kMinResizeGripSize,
                                        edge(m_resizeBorderHitSize),
                                        border.left,
                                        border.right,
                                        border.top,
                                        border.bottom});

        const bool left = hasResizeEdge(edges, UIPanelResizeEdge::left);
        const bool right = hasResizeEdge(edges, UIPanelResizeEdge::right);
        const bool top = hasResizeEdge(edges, UIPanelResizeEdge::top);
        const bool bottom = hasResizeEdge(edges, UIPanelResizeEdge::bottom);

        if (left && !right) {
            rect.right = std::min(rect.right, rect.left + hitSize);
        } else if (right && !left) {
            rect.left = std::max(rect.left, rect.right - hitSize);
        }

        if (top && !bottom) {
            rect.bottom = std::min(rect.bottom, rect.top + hitSize);
        } else if (bottom && !top) {
            rect.top = std::max(rect.top, rect.bottom - hitSize);
        }

        return rect;
    }

    void PanelComp::drawResizeHitRegion(SceneDrawContext &state,
                                        UIPanelResizeEdge edges,
                                        float zIndex) {
        if (!canResizeFromEdges(edges)) {
            return;
        }

        const Rect rect = resizeHitRect(edges);
        if (rectEmpty(rect)) {
            return;
        }

        Core::Renderer::QuadProps props;
        props.position = {
            rect.left + ((rect.right - rect.left) * 0.5f),
            rect.top + ((rect.bottom - rect.top) * 0.5f),
        };
        props.size = {rect.right - rect.left, rect.bottom - rect.top};
        props.zIndex = zIndex;
        props.color = Core::Renderer::Color{0.f, 0.f, 0.f, 0.f};
        props.id = PickingId{.runtimeId = resolveRuntimeId(),
                             .info = resizeInfo(edges)};
        props.transformMode = state.transformMode;
        state.renderer->drawQuad(props);
    }

    void PanelComp::updateSizeFromPointer(const glm::vec2 &pointerPos) {
        const glm::vec2 delta = pointerPos - m_resizeStartPointer;
        glm::vec2 requestedSize = m_resizeStartSize;

        if (hasResizeEdge(m_activeResizeEdges, UIPanelResizeEdge::left)) {
            requestedSize.x -= delta.x;
        } else if (hasResizeEdge(m_activeResizeEdges,
                                 UIPanelResizeEdge::right)) {
            requestedSize.x += delta.x;
        }

        if (hasResizeEdge(m_activeResizeEdges, UIPanelResizeEdge::top)) {
            requestedSize.y -= delta.y;
        } else if (hasResizeEdge(m_activeResizeEdges,
                                 UIPanelResizeEdge::bottom)) {
            requestedSize.y += delta.y;
        }

        const glm::vec2 nextSize = clampPanelSize(requestedSize);
        glm::vec3 nextPosition = m_resizeStartPosition;
        if (hasResizeEdge(m_activeResizeEdges, UIPanelResizeEdge::left)) {
            nextPosition.x += m_resizeStartSize.x - nextSize.x;
        }
        if (hasResizeEdge(m_activeResizeEdges, UIPanelResizeEdge::top)) {
            nextPosition.y += m_resizeStartSize.y - nextSize.y;
        }

        const bool positionChanged = nextPosition.x != m_transform.position.x ||
                                     nextPosition.y != m_transform.position.y ||
                                     nextPosition.z != m_transform.position.z;
        if (nextSize.x == m_panelSize.x && nextSize.y == m_panelSize.y) {
            if (positionChanged) {
                setPosition(nextPosition);
                makeUIDirty();
            }
            return;
        }

        m_panelSize = nextSize;
        if (positionChanged) {
            setPosition(nextPosition);
        }
        makeUIDirty();
        if (m_resizeCallback) {
            m_resizeCallback(m_panelSize);
        }
    }

    void PanelComp::onChildrenChanged() {
        makeUIDirty();
    }

    float PanelComp::resolveHeaderHeight(const glm::vec2 &titleSize) const {
        const auto padding = headerPadding();
        const float preferredHeight = titleSize.y + padding.vertical();
        return std::max(
            kMinHeaderHeight,
            std::max(finiteOr(m_headerHeight, 0.f), preferredHeight));
    }

    Core::Style::Padding PanelComp::resolvedBorderSize() const {
        return {
            edge(m_style.metrics.borderSize.top),
            edge(m_style.metrics.borderSize.right),
            edge(m_style.metrics.borderSize.bottom),
            edge(m_style.metrics.borderSize.left),
        };
    }

    Core::Style::Padding PanelComp::headerPadding() const {
        return {
            kHeaderPaddingY,
            kHeaderPaddingX,
            kHeaderPaddingY,
            kHeaderPaddingX,
        };
    }

    glm::vec2 PanelComp::resolvedMinPanelSize() const {
        const auto border = resolvedBorderSize();
        return {
            std::max({0.f,
                      finiteOr(m_minPanelSize.x, 0.f),
                      border.left + border.right + kMinInnerPanelWidth}),
            std::max({0.f,
                      finiteOr(m_minPanelSize.y, 0.f),
                      border.top + border.bottom + m_cachedHeaderHeight}),
        };
    }

    glm::vec2 PanelComp::resolvedMaxPanelSize(const glm::vec2 &minSize) const {
        glm::vec2 maxSize{-1.f, -1.f};
        if (std::isfinite(m_maxPanelSize.x) && m_maxPanelSize.x >= 0.f) {
            maxSize.x = std::max(minSize.x, m_maxPanelSize.x);
        }
        if (std::isfinite(m_maxPanelSize.y) && m_maxPanelSize.y >= 0.f) {
            maxSize.y = std::max(minSize.y, m_maxPanelSize.y);
        }
        return maxSize;
    }

    glm::vec2 PanelComp::clampPanelSize(const glm::vec2 &size) const {
        const glm::vec2 minSize = resolvedMinPanelSize();
        const glm::vec2 maxSize = resolvedMaxPanelSize(minSize);
        glm::vec2 next{
            std::max(minSize.x, finiteOr(size.x, minSize.x)),
            std::max(minSize.y, finiteOr(size.y, minSize.y)),
        };

        if (maxSize.x >= 0.f) {
            next.x = std::min(next.x, maxSize.x);
        }
        if (maxSize.y >= 0.f) {
            next.y = std::min(next.y, maxSize.y);
        }
        return next;
    }

    PanelComp::Rect PanelComp::innerPanelRect() const {
        return insetRect(nodeRect(m_node), resolvedBorderSize());
    }

    PanelComp::Rect PanelComp::nodeRect(const UINode *node) const {
        if (node == nullptr) {
            return {};
        }

        const auto center = node->getDrawPos();
        const auto size = node->getDrawSize();
        return {
            .left = center.x - (size.x * 0.5f),
            .top = center.y - (size.y * 0.5f),
            .right = center.x + (size.x * 0.5f),
            .bottom = center.y + (size.y * 0.5f),
        };
    }

    PanelComp::Rect
    PanelComp::insetRect(Rect rect, const Core::Style::Padding &insets) const {
        rect.left += edge(insets.left);
        rect.top += edge(insets.top);
        rect.right -= edge(insets.right);
        rect.bottom -= edge(insets.bottom);
        if (rect.right < rect.left) {
            rect.right = rect.left;
        }
        if (rect.bottom < rect.top) {
            rect.bottom = rect.top;
        }
        return rect;
    }

    PanelComp::Rect PanelComp::intersectRect(const Rect &lhs,
                                             const Rect &rhs) const {
        return {
            .left = std::max(lhs.left, rhs.left),
            .top = std::max(lhs.top, rhs.top),
            .right = std::max(std::max(lhs.left, rhs.left),
                              std::min(lhs.right, rhs.right)),
            .bottom = std::max(std::max(lhs.top, rhs.top),
                               std::min(lhs.bottom, rhs.bottom)),
        };
    }

    bool PanelComp::rectEmpty(const Rect &rect) const {
        return rect.right <= rect.left || rect.bottom <= rect.top;
    }

    bool PanelComp::pushClip(SceneDrawContext &state, const Rect &rect) const {
        if (state.renderer == nullptr || rectEmpty(rect) ||
            state.camera == nullptr) {
            return false;
        }

        const auto extent = state.camera->getSize();
        const float width = std::max(1.f, extent.x);
        const float height = std::max(1.f, extent.y);

        glm::vec2 minPixel{std::numeric_limits<float>::max()};
        glm::vec2 maxPixel{std::numeric_limits<float>::lowest()};
        const auto addPixel = [&](const glm::vec2 &pixel) {
            minPixel = glm::min(minPixel, pixel);
            maxPixel = glm::max(maxPixel, pixel);
        };

        if (state.transformMode ==
            Core::Renderer::RenderTransformMode::Screen) {
            addPixel({rect.left + (width * 0.5f), rect.top + (height * 0.5f)});
            addPixel(
                {rect.right + (width * 0.5f), rect.bottom + (height * 0.5f)});
        } else {
            const glm::mat4 transform = state.camera->getTransform();
            addPixel(clipToPixels(transform *
                                      glm::vec4(rect.left, rect.top, 0.f, 1.f),
                                  width,
                                  height));
            addPixel(clipToPixels(transform *
                                      glm::vec4(rect.right, rect.top, 0.f, 1.f),
                                  width,
                                  height));
            addPixel(clipToPixels(
                transform * glm::vec4(rect.left, rect.bottom, 0.f, 1.f),
                width,
                height));
            addPixel(clipToPixels(
                transform * glm::vec4(rect.right, rect.bottom, 0.f, 1.f),
                width,
                height));
        }

        const float left = std::clamp(std::floor(minPixel.x), 0.f, width);
        const float top = std::clamp(std::floor(minPixel.y), 0.f, height);
        const float right = std::clamp(std::ceil(maxPixel.x), 0.f, width);
        const float bottom = std::clamp(std::ceil(maxPixel.y), 0.f, height);
        if (right <= left || bottom <= top) {
            return false;
        }

        state.renderer->pushScissorRect({
            .x = static_cast<uint32_t>(left),
            .y = static_cast<uint32_t>(top),
            .width = static_cast<uint32_t>(right - left),
            .height = static_cast<uint32_t>(bottom - top),
        });
        return true;
    }
} // namespace Bess::Canvas::UI
