#include "bess_core/scene/scene_ui/controls/list_box_comp.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/camera.h"
#include "bess_core/scene/scene_event.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace Bess::Canvas::UI {
    namespace {
        constexpr uint32_t kViewportInfo = 1u;
        constexpr uint32_t kScrollbarTrackInfo = 2u;
        constexpr uint32_t kScrollbarThumbInfo = 3u;
        constexpr uint32_t kItemInfoBase = 100u;
        constexpr float kMinListWidth = 48.f;
        constexpr float kMinListHeight = 24.f;
        constexpr float kMinItemHeight = 14.f;
        constexpr float kScrollbarGap = 4.f;
        constexpr float kTextInset = 6.f;

        [[nodiscard]] float rectWidth(const ListBoxComp::Rect &rect) {
            return std::max(0.f, rect.right - rect.left);
        }

        [[nodiscard]] float rectHeight(const ListBoxComp::Rect &rect) {
            return std::max(0.f, rect.bottom - rect.top);
        }

        [[nodiscard]] bool rectEmpty(const ListBoxComp::Rect &rect) {
            return rectWidth(rect) <= 0.f || rectHeight(rect) <= 0.f;
        }

        [[nodiscard]] uint32_t itemInfo(size_t index) {
            const size_t maxIndex =
                static_cast<size_t>(std::numeric_limits<uint32_t>::max() -
                                    kItemInfoBase);
            if (index > maxIndex) {
                return 0u;
            }
            return kItemInfoBase + static_cast<uint32_t>(index);
        }

        [[nodiscard]] glm::vec2 clipToPixels(const glm::vec4 &clip,
                                             float width,
                                             float height) {
            const float invW = clip.w != 0.f ? 1.f / clip.w : 1.f;
            const float ndcX = clip.x * invW;
            const float ndcY = clip.y * invW;
            return {
                ((ndcX * 0.5f) + 0.5f) * width,
                (1.f - ((ndcY * 0.5f) + 0.5f)) * height,
            };
        }
    } // namespace

    std::shared_ptr<ListBoxComp>
    ListBoxComp::create(const std::vector<UIListBoxItem> &items,
                        size_t selectedIndex,
                        const UIListBoxCallback &changedCallback) {
        auto listBox = std::make_shared<ListBoxComp>();
        listBox->setItems(items);
        if (selectedIndex != noSelection) {
            listBox->setSelectedIndex(selectedIndex);
        }
        listBox->setChangedCallback(changedCallback);
        return listBox;
    }

    void ListBoxComp::setItems(const std::vector<UIListBoxItem> &items) {
        m_items = items;
        if (m_selectedIndex && *m_selectedIndex >= m_items.size()) {
            m_selectedIndex.reset();
        }
        clampScrollOffset();
        makeUIDirty();
    }

    const std::vector<UIListBoxItem> &ListBoxComp::getItems() const {
        return m_items;
    }

    std::optional<size_t> ListBoxComp::getSelectedIndex() const {
        return m_selectedIndex;
    }

    void ListBoxComp::setSelectedIndex(size_t index) {
        if (index >= m_items.size()) {
            return;
        }

        m_selectedIndex = index;
        ensureIndexVisible(index);
    }

    void ListBoxComp::clearSelection() {
        m_selectedIndex.reset();
    }

    void ListBoxComp::scrollToIndex(size_t index) {
        if (index >= m_items.size()) {
            return;
        }
        setScrollOffset(static_cast<float>(index) * itemHeight());
    }

    void ListBoxComp::scrollToSelection() {
        if (m_selectedIndex) {
            ensureIndexVisible(*m_selectedIndex);
        }
    }

    void ListBoxComp::onDraw(SceneDrawContext &state) {
        if (m_node == nullptr || state.renderer == nullptr) {
            return;
        }

        clampScrollOffset();
        drawBackground(state);

        Rect content = contentRect();
        Rect scroll = {};
        const bool scrollable =
            m_showScrollbar && hasScrollableContent(content);
        if (scrollable) {
            scroll = scrollbarRect(content);
            content.right = std::max(content.left, scroll.left - kScrollbarGap);
        }

        drawItems(state, content);
        if (scrollable) {
            drawScrollbar(state, content, scroll);
        }
    }

    void ListBoxComp::prepareUI(SceneUIPrepareCtx &state) {
        prepStyle(state.theme);
        initNode(state.sceneState->getUINodeRegistry());

        m_cachedListSize = resolveListSize(state);

        m_node->setDirection(LayoutDirection::vertical);
        m_node->setWidth(m_cachedListSize.x);
        m_node->setHeight(m_cachedListSize.y);
        m_node->setPadding(m_style.metrics.padding);
        m_node->setMargin(m_style.metrics.margin);
        m_node->setCrossAxisAlignment(LayoutAlignment::start);
        m_node->clearChildren();

        if (state.parentNode != nullptr) {
            state.parentNode->addChild(m_node);
        }

        clampScrollOffset();
        m_isUIDirty = false;
    }

    void ListBoxComp::prepStyle(
        const std::shared_ptr<Core::Style::BessTheme> &theme) {
        UISceneComponent::prepStyle(theme);

        const auto &colors = theme->getColorScheme().getColors();
        m_selectedTextColor = colors.onPrimary;
        m_disabledTextColor = colors.onSurfaceVariant.withAlpha(0.42f);
        m_selectedRowColor = colors.primary;
        m_scrollbarTrackColor = colors.surfaceContainerHigh.withAlpha(0.65f);
        m_scrollbarThumbColor = colors.outline.withAlpha(0.72f);
        m_scrollbarThumbHoverColor = colors.primary.withAlpha(0.85f);
    }

    bool ListBoxComp::onMouseEnter(const Events::MouseEnterEvent &e) {
        UISceneComponent::onMouseEnter(e);
        m_hoveredInfo = e.details;
        return true;
    }

    bool ListBoxComp::onMouseLeave(const Events::MouseLeaveEvent &e) {
        UISceneComponent::onMouseLeave(e);
        if (!m_draggingThumb) {
            m_hoveredInfo = 0u;
        }
        return true;
    }

    bool ListBoxComp::onMouseButton(const Events::MouseButtonEvent &e) {
        if (e.button != Events::MouseButton::left) {
            return false;
        }

        if (e.action == Events::MouseClickAction::release) {
            m_draggingThumb = false;
            return e.details == kScrollbarThumbInfo ||
                   e.details == kScrollbarTrackInfo || isItemInfo(e.details) ||
                   e.details == kViewportInfo;
        }

        if (e.action != Events::MouseClickAction::press) {
            return false;
        }

        if (isItemInfo(e.details)) {
            selectFromUser(itemIndexFromInfo(e.details));
            return true;
        }

        if (e.details == kScrollbarThumbInfo) {
            m_draggingThumb = true;
            m_dragStartPointerY = e.mousePos.y;
            m_dragStartScrollOffset = m_scrollOffset;
            return true;
        }

        if (e.details == kScrollbarTrackInfo) {
            const Rect content = contentRect();
            const Rect scroll = scrollbarRect(content);
            const float trackHeight = std::max(1.f, rectHeight(scroll));
            const float viewportHeight = std::max(1.f, rectHeight(content));
            const float totalHeight =
                std::max(viewportHeight, totalContentHeight());
            const float thumbHeight =
                std::clamp((viewportHeight / totalHeight) * trackHeight,
                           std::max(1.f, m_minThumbHeight),
                           trackHeight);
            const float thumbTravel = std::max(0.f, trackHeight - thumbHeight);
            const float thumbTop =
                scroll.top +
                ((maxScrollOffset() > 0.f
                      ? m_scrollOffset / maxScrollOffset()
                      : 0.f) *
                 thumbTravel);
            const float page = std::max(itemHeight(), rectHeight(content));
            scrollBy(e.mousePos.y < thumbTop ? -page : page);
            return true;
        }

        return e.details == kViewportInfo;
    }

    bool ListBoxComp::onMouseWheel(const Events::MouseWheelEvent &e) {
        if (m_items.empty()) {
            return false;
        }

        const float rows = std::max(0.25f, m_wheelScrollRows);
        scrollBy(-e.delta.y * itemHeight() * rows);
        return true;
    }

    bool ListBoxComp::onPointerMove(const Events::MouseMoveEvent &e) {
        if (!m_draggingThumb) {
            return false;
        }

        updateScrollFromThumbDrag(e.mousePos.y);
        return true;
    }

    bool ListBoxComp::hasPointerCapture() const {
        return m_draggingThumb;
    }

    bool ListBoxComp::isFocusable() const {
        return true;
    }

    bool ListBoxComp::wantsKeyboardInput() const {
        return m_focused;
    }

    bool ListBoxComp::onKeyEvent(const SceneEvent &evt) {
        if (evt.type != SceneEvent::Type::key ||
            (evt.data.keyPress.action != KeyAction::press &&
             evt.data.keyPress.action != KeyAction::hold)) {
            return false;
        }

        if (m_items.empty()) {
            return false;
        }

        const size_t current = m_selectedIndex.value_or(noSelection);
        const float pagePixels =
            std::max(itemHeight(), rectHeight(contentRect()));
        const size_t pageItems = std::max<size_t>(
            1, static_cast<size_t>(std::floor(pagePixels / itemHeight())));

        switch (evt.data.keyPress.keycode) {
        case KeyCode::arrowDown: {
            const size_t start =
                current == noSelection ? 0 : std::min(current + 1, m_items.size());
            const size_t next = nextEnabledIndex(start);
            if (next != noSelection) {
                selectFromUser(next);
            }
            return true;
        }
        case KeyCode::arrowUp: {
            const size_t start = current == noSelection
                                     ? m_items.size() - 1
                                     : (current > 0 ? current - 1 : 0);
            const size_t next = previousEnabledIndex(start);
            if (next != noSelection) {
                selectFromUser(next);
            }
            return true;
        }
        case KeyCode::pageDown: {
            if (current == noSelection) {
                const size_t next = nextEnabledIndex(0);
                if (next != noSelection) {
                    selectFromUser(next);
                }
            } else {
                const size_t target =
                    std::min(current + pageItems, m_items.size() - 1);
                const size_t next = previousEnabledIndex(target);
                if (next != noSelection) {
                    selectFromUser(next);
                }
            }
            return true;
        }
        case KeyCode::pageUp: {
            if (current == noSelection) {
                const size_t next = previousEnabledIndex(m_items.size() - 1);
                if (next != noSelection) {
                    selectFromUser(next);
                }
            } else {
                const size_t target = current > pageItems ? current - pageItems : 0;
                const size_t next = nextEnabledIndex(target);
                if (next != noSelection) {
                    selectFromUser(next);
                }
            }
            return true;
        }
        case KeyCode::home: {
            const size_t next = nextEnabledIndex(0);
            if (next != noSelection) {
                selectFromUser(next);
            }
            return true;
        }
        case KeyCode::end: {
            const size_t next = previousEnabledIndex(m_items.size() - 1);
            if (next != noSelection) {
                selectFromUser(next);
            }
            return true;
        }
        default:
            return false;
        }
    }

    Core::Viewport::SceneCursor ListBoxComp::getCursor() const {
        return Core::Viewport::SceneCursor::pointer;
    }

    void ListBoxComp::selectFromUser(size_t index) {
        if (index >= m_items.size() || !m_items[index].enabled) {
            return;
        }

        const bool changed = !m_selectedIndex || *m_selectedIndex != index;
        m_selectedIndex = index;
        ensureIndexVisible(index);

        if (changed && m_changedCallback) {
            m_changedCallback(index, m_items[index]);
        }
    }

    void ListBoxComp::setScrollOffset(float offset) {
        m_scrollOffset =
            std::clamp(std::isfinite(offset) ? offset : 0.f,
                       0.f,
                       maxScrollOffset());
    }

    void ListBoxComp::scrollBy(float delta) {
        setScrollOffset(m_scrollOffset + delta);
    }

    void ListBoxComp::clampScrollOffset() {
        setScrollOffset(m_scrollOffset);
    }

    void ListBoxComp::ensureIndexVisible(size_t index) {
        if (index >= m_items.size()) {
            return;
        }

        const Rect rect = contentRect();
        const float viewportHeight = rectHeight(rect);
        if (viewportHeight <= 0.f) {
            return;
        }

        const float height = itemHeight();
        const float itemTop = static_cast<float>(index) * height;
        const float itemBottom = itemTop + height;
        if (itemTop < m_scrollOffset) {
            setScrollOffset(itemTop);
        } else if (itemBottom > m_scrollOffset + viewportHeight) {
            setScrollOffset(itemBottom - viewportHeight);
        }
    }

    void ListBoxComp::updateScrollFromThumbDrag(float pointerY) {
        const Rect scroll = scrollbarRect(contentRect());
        const float trackHeight = std::max(1.f, rectHeight(scroll));
        const float viewportHeight = std::max(1.f, rectHeight(contentRect()));
        const float totalHeight = std::max(viewportHeight, totalContentHeight());
        const float thumbHeight =
            std::clamp((viewportHeight / totalHeight) * trackHeight,
                       std::max(1.f, m_minThumbHeight),
                       trackHeight);
        const float trackTravel = std::max(1.f, trackHeight - thumbHeight);
        const float maxScroll = maxScrollOffset();
        if (maxScroll <= 0.f) {
            setScrollOffset(0.f);
            return;
        }

        const float pointerDelta = pointerY - m_dragStartPointerY;
        setScrollOffset(m_dragStartScrollOffset +
                        ((pointerDelta / trackTravel) * maxScroll));
    }

    void ListBoxComp::drawBackground(SceneDrawContext &state) {
        const PickingId id{
            .runtimeId = resolveRuntimeId(),
            .info = kViewportInfo,
        };

        Core::Renderer::QuadProps props;
        props.position = m_node->getDrawPos();
        props.size = m_node->getDrawSize();
        props.zIndex = m_node->getDrawPos().z;
        props.color = m_style.backgroundColor;
        props.borderColor =
            m_focused ? m_style.activeColor : m_style.borderColor;
        props.thickness = m_style.metrics.borderSize.toVec4();
        props.radius = m_style.metrics.borderRadius;
        props.id = id;
        props.transformMode = state.transformMode;
        state.renderer->drawQuad(props);
    }

    void ListBoxComp::drawItems(SceneDrawContext &state,
                                const Rect &contentRect) {
        if (rectEmpty(contentRect)) {
            return;
        }

        const bool clipped = pushClip(state, contentRect);
        if (state.camera != nullptr && !clipped) {
            return;
        }
        const auto range = visibleRange(contentRect);
        const float rowWidth = std::max(1.f, rectWidth(contentRect));
        const float height = itemHeight();
        const float listZ = m_node->getDrawPos().z;

        for (size_t i = 0; i < range.count; ++i) {
            const size_t index = range.first + i;
            if (index >= m_items.size()) {
                break;
            }

            const auto &item = m_items[index];
            const uint32_t info = itemInfo(index);
            const bool interactive = item.enabled && info != 0u;
            const PickingId id = interactive
                                     ? PickingId{.runtimeId = resolveRuntimeId(),
                                                 .info = info}
                                     : PickingId::invalid();
            const float rowTop =
                range.firstTop + (static_cast<float>(i) * height);
            const glm::vec2 rowCenter{
                contentRect.left + (rowWidth * 0.5f),
                rowTop + (height * 0.5f),
            };

            const bool selected = m_selectedIndex && *m_selectedIndex == index;
            const bool hovered = interactive && m_hoveredInfo == info;
            Core::Renderer::QuadProps rowProps;
            rowProps.position = rowCenter;
            rowProps.size = {rowWidth, height};
            rowProps.zIndex = listZ + 0.0001f;
            rowProps.color = selected ? m_selectedRowColor
                             : hovered ? m_style.hoverColor
                                       : Core::Renderer::Color{0.f, 0.f, 0.f, 0.f};
            rowProps.id = id;
            rowProps.transformMode = state.transformMode;
            state.renderer->drawQuad(rowProps);

            const auto textColor =
                !item.enabled ? m_disabledTextColor
                : selected     ? m_selectedTextColor
                               : m_style.textStyle.textColor;
            const float offsetY = state.renderer->textCenterOffsetY(
                item.label, {.fontSize = m_style.textStyle.fontSize});
            const glm::vec2 textPos{
                contentRect.left + kTextInset,
                rowCenter.y + offsetY,
            };
            state.renderer->drawFont(
                item.label,
                {
                    .position = textPos,
                    .fontSize = m_style.textStyle.fontSize,
                    .color = textColor,
                    .zIndex = listZ + 0.0002f,
                    .id = id,
                    .transformMode = state.transformMode,
                });
        }

        if (clipped) {
            state.renderer->popScissorRect();
        }
    }

    void ListBoxComp::drawScrollbar(SceneDrawContext &state,
                                    const Rect &contentRect,
                                    const Rect &scrollbarRect) {
        if (rectEmpty(scrollbarRect)) {
            return;
        }

        const float maxScroll = maxScrollOffset();
        const float viewportHeight = std::max(1.f, rectHeight(contentRect));
        const float totalHeight = std::max(viewportHeight, totalContentHeight());
        const float trackHeight = rectHeight(scrollbarRect);
        const float thumbHeight =
            std::clamp((viewportHeight / totalHeight) * trackHeight,
                       std::max(1.f, m_minThumbHeight),
                       trackHeight);
        const float thumbTravel = std::max(0.f, trackHeight - thumbHeight);
        const float thumbTop =
            scrollbarRect.top +
            (maxScroll > 0.f ? (m_scrollOffset / maxScroll) * thumbTravel : 0.f);
        const float centerX =
            scrollbarRect.left + (rectWidth(scrollbarRect) * 0.5f);
        const float radius = std::max(1.f, rectWidth(scrollbarRect) * 0.5f);
        const float z = m_node->getDrawPos().z + 0.0003f;

        Core::Renderer::QuadProps track;
        track.position = {
            centerX,
            scrollbarRect.top + (trackHeight * 0.5f),
        };
        track.size = {rectWidth(scrollbarRect), trackHeight};
        track.zIndex = z;
        track.color = m_scrollbarTrackColor;
        track.radius = glm::vec4(radius);
        track.id = PickingId{
            .runtimeId = resolveRuntimeId(),
            .info = kScrollbarTrackInfo,
        };
        track.transformMode = state.transformMode;
        state.renderer->drawQuad(track);

        Core::Renderer::QuadProps thumb = track;
        thumb.position = {centerX, thumbTop + (thumbHeight * 0.5f)};
        thumb.size = {rectWidth(scrollbarRect), thumbHeight};
        thumb.zIndex = z + 0.0001f;
        thumb.color = (m_draggingThumb || m_hoveredInfo == kScrollbarThumbInfo)
                          ? m_scrollbarThumbHoverColor
                          : m_scrollbarThumbColor;
        thumb.id = PickingId{
            .runtimeId = resolveRuntimeId(),
            .info = kScrollbarThumbInfo,
        };
        state.renderer->drawQuad(thumb);
    }

    glm::vec2 ListBoxComp::resolveListSize(SceneUIPrepareCtx &state) const {
        glm::vec2 size = m_listSize;
        if (size.x <= 0.f) {
            float maxTextWidth = 0.f;
            for (const auto &item : m_items) {
                maxTextWidth = std::max(
                    maxTextWidth,
                    state.renderer
                        ->measureText(item.label,
                                      {.fontSize = m_style.textStyle.fontSize})
                        .x);
            }
            size.x = maxTextWidth + kTextInset + m_style.metrics.padding.right +
                     std::max(0.f, m_scrollbarWidth) + kScrollbarGap;
        }
        if (size.y <= 0.f) {
            const size_t visible =
                std::min<size_t>(m_items.empty() ? 1 : m_items.size(), 5);
            size.y = (static_cast<float>(visible) * itemHeight()) +
                     m_style.metrics.padding.vertical();
        }

        size.x = std::max(kMinListWidth, size.x);
        size.y = std::max(kMinListHeight, size.y);
        return size;
    }

    ListBoxComp::Rect ListBoxComp::nodeRect() const {
        if (m_node == nullptr) {
            return {};
        }

        const auto center = m_node->getDrawPos();
        const auto size = m_node->getDrawSize();
        return {
            .left = center.x - (size.x * 0.5f),
            .top = center.y - (size.y * 0.5f),
            .right = center.x + (size.x * 0.5f),
            .bottom = center.y + (size.y * 0.5f),
        };
    }

    ListBoxComp::Rect ListBoxComp::contentRect() const {
        Rect rect = nodeRect();
        if (rectEmpty(rect)) {
            rect = {
                .left = 0.f,
                .top = 0.f,
                .right = std::max(0.f, m_cachedListSize.x),
                .bottom = std::max(0.f, m_cachedListSize.y),
            };
        }

        rect.left += m_style.metrics.borderSize.left + m_style.metrics.padding.left;
        rect.top += m_style.metrics.borderSize.top + m_style.metrics.padding.top;
        rect.right -=
            m_style.metrics.borderSize.right + m_style.metrics.padding.right;
        rect.bottom -=
            m_style.metrics.borderSize.bottom + m_style.metrics.padding.bottom;
        if (rect.right < rect.left) {
            rect.right = rect.left;
        }
        if (rect.bottom < rect.top) {
            rect.bottom = rect.top;
        }
        return rect;
    }

    ListBoxComp::Rect
    ListBoxComp::scrollbarRect(const Rect &contentRect) const {
        const float width = std::clamp(m_scrollbarWidth, 4.f, 18.f);
        const float right = contentRect.right;
        return {
            .left = std::max(contentRect.left, right - width),
            .top = contentRect.top,
            .right = right,
            .bottom = contentRect.bottom,
        };
    }

    float ListBoxComp::itemHeight() const {
        return std::max(kMinItemHeight, m_itemHeight);
    }

    float ListBoxComp::totalContentHeight() const {
        return static_cast<float>(m_items.size()) * itemHeight();
    }

    float ListBoxComp::maxScrollOffset() const {
        const float viewportHeight = rectHeight(contentRect());
        return std::max(0.f, totalContentHeight() - viewportHeight);
    }

    bool ListBoxComp::hasScrollableContent(const Rect &contentRect) const {
        return totalContentHeight() > rectHeight(contentRect) + 0.5f;
    }

    ListBoxComp::VisibleRange
    ListBoxComp::visibleRange(const Rect &contentRect) const {
        VisibleRange range{};
        if (m_items.empty() || rectEmpty(contentRect)) {
            return range;
        }

        const float height = itemHeight();
        const float viewportHeight = rectHeight(contentRect);
        range.first = static_cast<size_t>(
            std::floor(std::max(0.f, m_scrollOffset) / height));
        range.first = std::min(range.first, m_items.size() - 1);
        const float offsetIntoFirst =
            m_scrollOffset - (static_cast<float>(range.first) * height);
        range.firstTop = contentRect.top - offsetIntoFirst;
        const float visibleHeight = viewportHeight + offsetIntoFirst;
        range.count =
            static_cast<size_t>(std::ceil(visibleHeight / height)) + 1u;
        range.count = std::min(range.count, m_items.size() - range.first);
        return range;
    }

    bool ListBoxComp::isItemInfo(uint32_t info) const {
        return info >= kItemInfoBase && itemIndexFromInfo(info) < m_items.size();
    }

    size_t ListBoxComp::itemIndexFromInfo(uint32_t info) const {
        return static_cast<size_t>(info - kItemInfoBase);
    }

    size_t ListBoxComp::nextEnabledIndex(size_t start) const {
        for (size_t i = start; i < m_items.size(); ++i) {
            if (m_items[i].enabled) {
                return i;
            }
        }
        return noSelection;
    }

    size_t ListBoxComp::previousEnabledIndex(size_t start) const {
        if (m_items.empty()) {
            return noSelection;
        }

        size_t i = std::min(start, m_items.size() - 1);
        for (;;) {
            if (m_items[i].enabled) {
                return i;
            }
            if (i == 0) {
                break;
            }
            --i;
        }
        return noSelection;
    }

    bool ListBoxComp::pushClip(SceneDrawContext &state, const Rect &rect) const {
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

        if (state.transformMode == Core::Renderer::RenderTransformMode::Screen) {
            addPixel({rect.left + (width * 0.5f),
                      rect.top + (height * 0.5f)});
            addPixel({rect.right + (width * 0.5f),
                      rect.bottom + (height * 0.5f)});
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
            addPixel(clipToPixels(transform *
                                      glm::vec4(rect.left, rect.bottom, 0.f, 1.f),
                                  width,
                                  height));
            addPixel(clipToPixels(transform *
                                      glm::vec4(rect.right, rect.bottom, 0.f, 1.f),
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
