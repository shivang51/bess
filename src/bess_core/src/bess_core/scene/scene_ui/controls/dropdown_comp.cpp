#include "bess_core/scene/scene_ui/controls/dropdown_comp.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/scene_event.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include <algorithm>

namespace Bess::Canvas::UI {
    namespace {
        constexpr uint32_t kDropdownHeaderInfo = 1u;
        constexpr uint32_t kDropdownOptionInfoBase = 100u;
        constexpr float kChevronWidth = 12.f;
        constexpr float kMinHeaderWidth = 48.f;
        constexpr float kMinItemHeight = 16.f;

        [[nodiscard]] Events::MouseButton
        toEventButton(Events::MouseButton button) {
            return button;
        }

        [[nodiscard]] Core::Renderer::Color
        disabledTextColor(const Core::Renderer::Color &color) {
            return color.withAlpha(0.42f);
        }
    } // namespace

    std::shared_ptr<DropdownComp>
    DropdownComp::create(const std::vector<UIDropdownOption> &options,
                         size_t selectedIndex,
                         const UIDropdownCallback &changedCallback) {
        auto dropdown = std::make_shared<DropdownComp>();
        dropdown->setOptions(options);
        dropdown->setSelectedIndex(selectedIndex);
        dropdown->setChangedCallback(changedCallback);
        return dropdown;
    }

    void
    DropdownComp::setOptions(const std::vector<UIDropdownOption> &options) {
        m_options = options;
        m_selectedIndex = validSelectedIndex(m_selectedIndex);
        m_scrollOffset = 0;
        ensureSelectedVisible();
        makeUIDirty();
    }

    const std::vector<UIDropdownOption> &DropdownComp::getOptions() const {
        return m_options;
    }

    size_t DropdownComp::getSelectedIndex() const {
        return m_selectedIndex;
    }

    void DropdownComp::setSelectedIndex(size_t index) {
        const auto next = validSelectedIndex(index);
        if (m_selectedIndex == next) {
            return;
        }

        m_selectedIndex = next;
        ensureSelectedVisible();
        makeUIDirty();
    }

    void DropdownComp::open() {
        if (m_open || m_options.empty()) {
            return;
        }
        m_open = true;
        ensureSelectedVisible();
    }

    void DropdownComp::close() {
        if (!m_open) {
            return;
        }
        m_open = false;
        m_hoveredInfo = 0u;
    }

    void DropdownComp::toggleOpen() {
        if (m_open) {
            close();
        } else {
            open();
        }
    }

    void DropdownComp::onDraw(SceneDrawContext &state) {
        if (m_node == nullptr || state.renderer == nullptr) {
            return;
        }

        const PickingId headerId{
            .runtimeId = resolveRuntimeId(),
            .info = kDropdownHeaderInfo,
        };

        Core::Renderer::QuadProps headerProps;
        headerProps.position = m_node->getDrawPos();
        headerProps.size = m_node->getDrawSize();
        headerProps.zIndex = m_node->getDrawPos().z;
        headerProps.color =
            (m_hoveredInfo == 0u || m_hoveredInfo == kDropdownHeaderInfo)
                ? m_style.hoverColor
                : m_style.backgroundColor;
        headerProps.borderColor =
            m_focused ? m_style.activeColor : m_style.borderColor;
        headerProps.thickness = m_style.metrics.borderSize.toVec4();
        headerProps.radius = m_style.metrics.borderRadius;
        headerProps.id = headerId;
        headerProps.transformMode = state.transformMode;
        state.renderer->drawQuad(headerProps);

        drawText(state, selectedLabel(), m_labelNode);
        drawChevron(state, m_chevronNode);
        drawMenu(state);
    }

    void DropdownComp::prepareUI(SceneUIPrepareCtx &state) {
        prepStyle(state.theme);
        initNode(state.sceneState->getUINodeRegistry());

        if (m_labelNode == nullptr) {
            m_labelNode =
                state.sceneState->getUINodeRegistry()->addNode(UUID());
        }
        if (m_chevronNode == nullptr) {
            m_chevronNode =
                state.sceneState->getUINodeRegistry()->addNode(UUID());
        }

        m_cachedHeaderSize = resolveHeaderSize(state);
        m_cachedMenuWidth = resolveMenuWidth();

        m_node->setDirection(LayoutDirection::horizontal);
        m_node->setWidth(m_cachedHeaderSize.x);
        m_node->setHeight(m_cachedHeaderSize.y);
        m_node->setCrossAxisAlignment(LayoutAlignment::center);
        m_node->setPadding(m_style.metrics.padding);
        m_node->setMargin(m_style.metrics.margin);

        const auto labelWidth = std::max(
            1.f,
            m_cachedHeaderSize.x - m_style.metrics.padding.horizontal() -
                kChevronWidth - 6.f);
        m_labelNode->setWidth(labelWidth);
        m_labelNode->setHeight(std::max(
            1.f, m_cachedHeaderSize.y - m_style.metrics.padding.vertical()));
        m_labelNode->setPosMode(PosMode::relative);
        m_labelNode->setPadding(0.f);
        m_labelNode->setMargin(Core::Style::Margin::onlyRight(6.f));
        m_node->addChild(m_labelNode);

        m_chevronNode->setWidth(kChevronWidth);
        m_chevronNode->setHeight(kChevronWidth);
        m_chevronNode->setPosMode(PosMode::relative);
        m_chevronNode->setPadding(0.f);
        m_chevronNode->setMargin(0.f);
        m_node->addChild(m_chevronNode);

        if (state.parentNode != nullptr) {
            state.parentNode->addChild(m_node);
        }

        m_isUIDirty = false;
    }

    bool DropdownComp::onMouseEnter(const Events::MouseEnterEvent &e) {
        UISceneComponent::onMouseEnter(e);
        m_hoveredInfo = e.details;
        return true;
    }

    bool DropdownComp::onMouseLeave(const Events::MouseLeaveEvent &e) {
        UISceneComponent::onMouseLeave(e);
        m_hoveredInfo = 0u;
        return true;
    }

    bool DropdownComp::onMouseButton(const Events::MouseButtonEvent &e) {
        if (toEventButton(e.button) != Events::MouseButton::left) {
            return false;
        }

        if (e.action != Events::MouseClickAction::press) {
            return e.action == Events::MouseClickAction::release;
        }

        if (e.details == 0u || e.details == kDropdownHeaderInfo) {
            toggleOpen();
            return true;
        }

        if (isOptionInfo(e.details)) {
            selectFromUser(optionIndexFromInfo(e.details));
            return true;
        }

        return false;
    }

    bool DropdownComp::onMouseWheel(const Events::MouseWheelEvent &e) {
        if (!m_open || m_options.empty()) {
            return false;
        }

        const auto visible =
            std::max<size_t>(1, std::min(m_maxVisibleItems, m_options.size()));
        const auto maxOffset =
            m_options.size() > visible ? m_options.size() - visible : 0;
        if (maxOffset == 0) {
            return true;
        }

        if (e.delta.y > 0.f && m_scrollOffset > 0) {
            --m_scrollOffset;
        } else if (e.delta.y < 0.f && m_scrollOffset < maxOffset) {
            ++m_scrollOffset;
        }

        return true;
    }

    bool DropdownComp::isFocusable() const {
        return true;
    }

    bool DropdownComp::wantsKeyboardInput() const {
        return m_focused;
    }

    void DropdownComp::onFocusLost(const Events::FocusEvent &e) {
        UISceneComponent::onFocusLost(e);
        close();
    }

    bool DropdownComp::onKeyEvent(const SceneEvent &evt) {
        if (evt.type != SceneEvent::Type::key ||
            evt.data.keyPress.action != KeyAction::press) {
            return false;
        }

        if (evt.data.keyPress.keycode == KeyCode::escape) {
            close();
            return true;
        }

        if (evt.data.keyPress.keycode == KeyCode::space ||
            evt.data.keyPress.keycode == KeyCode::enter) {
            if (m_open && !m_options.empty()) {
                selectFromUser(m_selectedIndex);
            } else {
                open();
            }
            return true;
        }

        if (m_options.empty()) {
            return false;
        }

        if (evt.data.keyPress.keycode == KeyCode::arrowDown) {
            open();
            setSelectedIndex(
                std::min(m_selectedIndex + 1, m_options.size() - 1));
            return true;
        }

        if (evt.data.keyPress.keycode == KeyCode::arrowUp) {
            open();
            setSelectedIndex(m_selectedIndex > 0 ? m_selectedIndex - 1 : 0);
            return true;
        }

        return false;
    }

    std::string DropdownComp::selectedLabel() const {
        if (m_options.empty()) {
            return m_placeholder;
        }
        return m_options[validSelectedIndex(m_selectedIndex)].label;
    }

    size_t DropdownComp::validSelectedIndex(size_t index) const {
        if (m_options.empty()) {
            return 0;
        }
        return std::min(index, m_options.size() - 1);
    }

    bool DropdownComp::isOptionInfo(uint32_t info) const {
        if (info < kDropdownOptionInfoBase) {
            return false;
        }
        return optionIndexFromInfo(info) < m_options.size();
    }

    size_t DropdownComp::optionIndexFromInfo(uint32_t info) const {
        return static_cast<size_t>(info - kDropdownOptionInfoBase);
    }

    glm::vec2 DropdownComp::resolveHeaderSize(SceneUIPrepareCtx &state) {
        auto size = m_headerSize;
        float maxTextWidth =
            state.renderer
                ->measureText(m_placeholder,
                              {.fontSize = m_style.textStyle.fontSize})
                .x;
        float maxTextHeight =
            state.renderer
                ->measureText("M", {.fontSize = m_style.textStyle.fontSize})
                .y;

        for (const auto &option : m_options) {
            const auto measured = state.renderer->measureText(
                option.label,
                {
                    .fontSize = m_style.textStyle.fontSize,
                });
            maxTextWidth = std::max(maxTextWidth, measured.x);
            maxTextHeight = std::max(maxTextHeight, measured.y);
        }

        if (size.x <= 0.f) {
            size.x = maxTextWidth + kChevronWidth + 6.f +
                     m_style.metrics.padding.horizontal();
        }
        if (size.y <= 0.f) {
            size.y = maxTextHeight + m_style.metrics.padding.vertical();
        }

        size.x = std::max(kMinHeaderWidth, size.x);
        size.y = std::max(kMinItemHeight, size.y);
        return size;
    }

    float DropdownComp::resolveMenuWidth() const {
        return std::max(m_cachedHeaderSize.x,
                        m_menuWidth > 0.f ? m_menuWidth : m_cachedHeaderSize.x);
    }

    void DropdownComp::selectFromUser(size_t index) {
        if (index >= m_options.size() || !m_options[index].enabled) {
            return;
        }

        const bool changed = index != m_selectedIndex;
        m_selectedIndex = index;
        ensureSelectedVisible();
        close();

        if (changed && m_changedCallback) {
            m_changedCallback(m_selectedIndex, m_options[m_selectedIndex]);
        }
    }

    void DropdownComp::ensureSelectedVisible() {
        if (m_options.empty()) {
            m_scrollOffset = 0;
            return;
        }

        const auto visible =
            std::max<size_t>(1, std::min(m_maxVisibleItems, m_options.size()));
        if (m_selectedIndex < m_scrollOffset) {
            m_scrollOffset = m_selectedIndex;
        } else if (m_selectedIndex >= m_scrollOffset + visible) {
            m_scrollOffset = m_selectedIndex - visible + 1;
        }

        const auto maxOffset =
            m_options.size() > visible ? m_options.size() - visible : 0;
        m_scrollOffset = std::min(m_scrollOffset, maxOffset);
    }

    void DropdownComp::drawChevron(SceneDrawContext &state,
                                   const UINode *node) {
        if (node == nullptr || state.renderer == nullptr) {
            return;
        }

        const auto center = node->getDrawPos();
        const auto size = node->getDrawSize();
        const float half = std::min(size.x, size.y) * 0.25f;
        const float thickness = std::max(1.f, std::min(size.x, size.y) * 0.12f);
        const PickingId id{
            .runtimeId = resolveRuntimeId(),
            .info = kDropdownHeaderInfo,
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
        if (m_open) {
            state.renderer->pathMoveTo(
                {center.x - half, center.y - (half * 0.4f)});
            state.renderer->pathLineTo(
                {center.x, center.y + (half * 0.55f)}, thickness, id);
            state.renderer->pathLineTo(
                {center.x + half, center.y - (half * 0.4f)}, thickness, id);
            state.renderer->endPath();
            return;
        }

        state.renderer->pathMoveTo(
            {center.x - half, center.y + (half * 0.45f)});
        state.renderer->pathLineTo(
            {center.x, center.y - (half * 0.45f)}, thickness, id);
        state.renderer->pathLineTo(
            {center.x + half, center.y + (half * 0.45f)}, thickness, id);
        state.renderer->endPath();
    }

    void DropdownComp::drawMenu(SceneDrawContext &state) {
        if (!m_open || m_options.empty() || state.renderer == nullptr) {
            return;
        }

        const auto visible =
            std::max<size_t>(1, std::min(m_maxVisibleItems, m_options.size()));
        const auto itemHeight = std::max(kMinItemHeight, m_itemHeight);
        const auto headerPos = m_node->getDrawPos();
        const auto headerSize = m_node->getDrawSize();
        const auto menuWidth = std::max(m_cachedMenuWidth, headerSize.x);
        const auto borderInset = std::max({m_style.metrics.borderSize.top,
                                           m_style.metrics.borderSize.right,
                                           m_style.metrics.borderSize.bottom,
                                           m_style.metrics.borderSize.left,
                                           1.f});
        const auto contentInset = borderInset + 1.f;
        const auto contentWidth =
            std::max(1.f, menuWidth - (contentInset * 2.f));
        const auto menuHeight =
            (itemHeight * static_cast<float>(visible)) + (contentInset * 2.f);
        const auto topY = headerPos.y + (headerSize.y * 0.5f);
        const auto menuCenter = glm::vec2{
            headerPos.x + ((menuWidth - headerSize.x) * 0.5f),
            topY + (menuHeight * 0.5f),
        };
        const float menuZ = headerPos.z + 0.05f;

        Core::Renderer::QuadProps menuProps;
        menuProps.position = menuCenter;
        menuProps.size = {menuWidth, menuHeight};
        menuProps.zIndex = menuZ;
        menuProps.color = m_style.backgroundColor;
        menuProps.borderColor = m_style.borderColor;
        menuProps.thickness = m_style.metrics.borderSize.toVec4();
        menuProps.radius = m_style.metrics.borderRadius;
        menuProps.id = PickingId::invalid();
        menuProps.transformMode = state.transformMode;
        state.renderer->drawQuad(menuProps);

        const auto textColor = m_style.textStyle.textColor;
        for (size_t visibleIndex = 0; visibleIndex < visible; ++visibleIndex) {
            const auto optionIndex = m_scrollOffset + visibleIndex;
            if (optionIndex >= m_options.size()) {
                break;
            }

            const auto &option = m_options[optionIndex];
            const auto info =
                kDropdownOptionInfoBase + static_cast<uint32_t>(optionIndex);
            const PickingId id{
                .runtimeId = resolveRuntimeId(),
                .info = info,
            };
            const auto itemCenter = glm::vec2{
                menuCenter.x,
                topY + contentInset +
                    (itemHeight * (static_cast<float>(visibleIndex) + 0.5f)),
            };

            const bool selected = optionIndex == m_selectedIndex;
            const bool hovered = m_hoveredInfo == info;
            Core::Renderer::QuadProps itemProps;
            itemProps.position = itemCenter;
            itemProps.size = {contentWidth, itemHeight};
            itemProps.zIndex = menuZ + 0.0001f;
            itemProps.color = selected ? m_style.activeColor.withAlpha(0.18f)
                              : hovered && option.enabled
                                  ? m_style.hoverColor
                                  : Core::Renderer::Color{0.f, 0.f, 0.f, 0.f};
            itemProps.id = id;
            itemProps.transformMode = state.transformMode;
            state.renderer->drawQuad(itemProps);

            const auto textOffsetY = state.renderer->textCenterOffsetY(
                option.label,
                {
                    .fontSize = m_style.textStyle.fontSize,
                });
            const auto textPos = glm::vec2{
                itemCenter.x - (contentWidth * 0.5f) +
                    m_style.metrics.padding.left,
                itemCenter.y + textOffsetY,
            };
            state.renderer->drawFont(
                option.label,
                {
                    .position = textPos,
                    .fontSize = m_style.textStyle.fontSize,
                    .color = option.enabled ? textColor
                                            : disabledTextColor(textColor),
                    .zIndex = menuZ + 0.0002f,
                    .id = id,
                    .transformMode = state.transformMode,
                });
        }
    }
} // namespace Bess::Canvas::UI
