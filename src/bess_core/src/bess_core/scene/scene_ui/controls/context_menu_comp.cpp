#include "bess_core/scene/scene_ui/controls/context_menu_comp.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/scene_event.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include <algorithm>

namespace Bess::Canvas::UI {
    namespace {
        constexpr uint32_t kContextTriggerInfo = 1u;
        constexpr uint32_t kContextItemInfoBase = 100u;
        constexpr float kMinMenuWidth = 80.f;
        constexpr float kMinItemHeight = 16.f;
        constexpr float kSeparatorHeight = 7.f;

        [[nodiscard]] Core::Renderer::Color
        disabledTextColor(const Core::Renderer::Color &color) {
            return color.withAlpha(0.42f);
        }
    } // namespace

    std::shared_ptr<ContextMenuComp>
    ContextMenuComp::create(const std::vector<UIContextMenuItem> &items,
                            const std::string &triggerLabel) {
        auto menu = std::make_shared<ContextMenuComp>();
        menu->setItems(items);
        menu->setTriggerLabel(triggerLabel);
        return menu;
    }

    void
    ContextMenuComp::setItems(const std::vector<UIContextMenuItem> &items) {
        m_items = items;
        makeUIDirty();
    }

    const std::vector<UIContextMenuItem> &ContextMenuComp::getItems() const {
        return m_items;
    }

    void ContextMenuComp::showAt(const glm::vec2 &position) {
        m_menuPos = position;
        m_open = !m_items.empty();
        m_hoveredInfo = 0u;
    }

    void ContextMenuComp::hide() {
        m_open = false;
        m_hoveredInfo = 0u;
    }

    void ContextMenuComp::onDraw(SceneDrawContext &state) {
        if (m_node == nullptr || state.renderer == nullptr) {
            return;
        }

        if (m_showTrigger) {
            const PickingId triggerId{
                .runtimeId = resolveRuntimeId(),
                .info = kContextTriggerInfo,
            };

            Core::Renderer::QuadProps triggerProps;
            triggerProps.position = m_node->getDrawPos();
            triggerProps.size = m_node->getDrawSize();
            triggerProps.zIndex = m_node->getDrawPos().z;
            triggerProps.color =
                (m_hoveredInfo == 0u || m_hoveredInfo == kContextTriggerInfo)
                    ? m_style.hoverColor
                    : m_style.backgroundColor;
            triggerProps.borderColor = m_style.borderColor;
            triggerProps.thickness = m_style.metrics.borderSize.toVec4();
            triggerProps.radius = m_style.metrics.borderRadius;
            triggerProps.id = triggerId;
            triggerProps.transformMode = state.transformMode;
            state.renderer->drawQuad(triggerProps);

            drawText(state, m_triggerLabel, m_labelNode);
        }

        drawMenu(state);
    }

    void ContextMenuComp::prepareUI(SceneUIPrepareCtx &state) {
        prepStyle(state.theme);
        initNode(state.sceneState->getUINodeRegistry());

        if (m_labelNode == nullptr) {
            m_labelNode =
                state.sceneState->getUINodeRegistry()->addNode(UUID());
        }

        m_cachedMenuWidth = resolveMenuWidth(state);

        if (!m_showTrigger) {
            m_node->setWidth(0.f);
            m_node->setHeight(0.f);
            m_node->setPadding(0.f);
            m_node->setMargin(0.f);
        } else {
            auto size = m_triggerSize;
            const auto textSize = state.renderer->measureText(
                m_triggerLabel,
                {
                    .fontSize = m_style.textStyle.fontSize,
                });
            if (size.x <= 0.f) {
                size.x = textSize.x + m_style.metrics.padding.horizontal();
            }
            if (size.y <= 0.f) {
                size.y = textSize.y + m_style.metrics.padding.vertical();
            }

            m_node->setDirection(LayoutDirection::horizontal);
            m_node->setWidth(std::max(1.f, size.x));
            m_node->setHeight(std::max(1.f, size.y));
            m_node->setCrossAxisAlignment(LayoutAlignment::center);
            m_node->setPadding(m_style.metrics.padding);
            m_node->setMargin(m_style.metrics.margin);

            m_labelNode->setWidth(textSize.x);
            m_labelNode->setHeight(textSize.y);
            m_labelNode->setPosMode(PosMode::relative);
            m_labelNode->setPadding(0.f);
            m_labelNode->setMargin(0.f);
            m_node->addChild(m_labelNode);
        }

        applyCustomLayoutStyle();

        if (state.parentNode != nullptr) {
            state.parentNode->addChild(m_node);
        }

        m_isUIDirty = false;
    }

    bool ContextMenuComp::onMouseEnter(const Events::MouseEnterEvent &e) {
        UISceneComponent::onMouseEnter(e);
        m_hoveredInfo = e.details;
        return true;
    }

    bool ContextMenuComp::onMouseLeave(const Events::MouseLeaveEvent &e) {
        UISceneComponent::onMouseLeave(e);
        m_hoveredInfo = 0u;
        return true;
    }

    bool ContextMenuComp::onMouseButton(const Events::MouseButtonEvent &e) {
        if (e.action != Events::MouseClickAction::press) {
            return e.action == Events::MouseClickAction::release;
        }

        if (e.button == Events::MouseButton::right &&
            (e.details == 0u || e.details == kContextTriggerInfo)) {
            showAt(e.mousePos);
            return true;
        }

        if (e.button == Events::MouseButton::left && isItemInfo(e.details)) {
            activateItem(itemIndexFromInfo(e.details));
            return true;
        }

        if (e.button == Events::MouseButton::right && m_open) {
            showAt(e.mousePos);
            return true;
        }

        return false;
    }

    bool ContextMenuComp::isFocusable() const {
        return true;
    }

    bool ContextMenuComp::wantsKeyboardInput() const {
        return m_focused;
    }

    void ContextMenuComp::onFocusLost(const Events::FocusEvent &e) {
        UISceneComponent::onFocusLost(e);
        hide();
    }

    bool ContextMenuComp::onKeyEvent(const SceneEvent &evt) {
        if (evt.type != SceneEvent::Type::key ||
            evt.data.keyPress.action != KeyAction::press) {
            return false;
        }

        if (evt.data.keyPress.keycode == KeyCode::escape) {
            hide();
            return true;
        }

        return false;
    }

    bool ContextMenuComp::isItemInfo(uint32_t info) const {
        if (info < kContextItemInfoBase) {
            return false;
        }
        return itemIndexFromInfo(info) < m_items.size();
    }

    size_t ContextMenuComp::itemIndexFromInfo(uint32_t info) const {
        return static_cast<size_t>(info - kContextItemInfoBase);
    }

    float ContextMenuComp::resolveMenuWidth(SceneUIPrepareCtx &state) const {
        float width = m_menuWidth > 0.f ? m_menuWidth : kMinMenuWidth;
        for (const auto &item : m_items) {
            const auto measured = state.renderer->measureText(
                item.label,
                {
                    .fontSize = m_style.textStyle.fontSize,
                });
            width = std::max(width,
                             measured.x + m_style.metrics.padding.horizontal());
        }
        return std::max(kMinMenuWidth, width);
    }

    void ContextMenuComp::activateItem(size_t index) {
        if (index >= m_items.size()) {
            return;
        }

        auto &item = m_items[index];
        if (!item.enabled || item.separator) {
            return;
        }

        hide();
        if (item.callback) {
            item.callback();
        }
    }

    void ContextMenuComp::drawMenu(SceneDrawContext &state) {
        if (!m_open || m_items.empty() || state.renderer == nullptr) {
            return;
        }

        const auto itemHeight = std::max(kMinItemHeight, m_itemHeight);
        const auto width = std::max(kMinMenuWidth, m_cachedMenuWidth);
        const auto borderInset = std::max({m_style.metrics.borderSize.top,
                                           m_style.metrics.borderSize.right,
                                           m_style.metrics.borderSize.bottom,
                                           m_style.metrics.borderSize.left,
                                           1.f});
        const auto contentInset = borderInset + 1.f;
        const auto contentWidth = std::max(1.f, width - (contentInset * 2.f));
        float height = 0.f;
        for (const auto &item : m_items) {
            height += item.separator ? kSeparatorHeight : itemHeight;
        }
        height += contentInset * 2.f;
        const auto menuCenter = glm::vec2{m_menuPos.x + (width * 0.5f),
                                          m_menuPos.y + (height * 0.5f)};
        const auto z = m_node != nullptr ? m_node->getDrawPos().z + 0.08f
                                         : m_transform.position.z + 0.08f;

        Core::Renderer::QuadProps menuProps;
        menuProps.position = menuCenter;
        menuProps.size = {width, height};
        menuProps.zIndex = z;
        menuProps.color = m_style.backgroundColor;
        menuProps.borderColor = m_style.borderColor;
        menuProps.thickness = m_style.metrics.borderSize.toVec4();
        menuProps.radius = m_style.metrics.borderRadius;
        menuProps.id = PickingId::invalid();
        menuProps.transformMode = state.transformMode;
        state.renderer->drawQuad(menuProps);

        const auto textColor = m_style.textStyle.textColor;
        float rowTop = m_menuPos.y + contentInset;
        for (size_t index = 0; index < m_items.size(); ++index) {
            const auto &item = m_items[index];
            const auto rowHeight =
                item.separator ? kSeparatorHeight : itemHeight;
            const auto info =
                kContextItemInfoBase + static_cast<uint32_t>(index);
            const PickingId id{
                .runtimeId = resolveRuntimeId(),
                .info = info,
            };
            const auto itemCenter =
                glm::vec2{menuCenter.x, rowTop + (rowHeight * 0.5f)};

            if (item.separator) {
                Core::Renderer::LineProps sep;
                sep.p0 = {m_menuPos.x + contentInset + 6.f, itemCenter.y};
                sep.p1 = {m_menuPos.x + width - contentInset - 6.f,
                          itemCenter.y};
                sep.thickness = 1.f;
                sep.zIndex = z + 0.0001f;
                sep.color = m_style.borderColor.withAlpha(0.72f);
                sep.id = PickingId::invalid();
                sep.transformMode = state.transformMode;
                state.renderer->drawLine(sep);
                rowTop += rowHeight;
                continue;
            }

            const bool hovered = m_hoveredInfo == info && item.enabled;
            Core::Renderer::QuadProps itemProps;
            itemProps.position = itemCenter;
            itemProps.size = {contentWidth, rowHeight};
            itemProps.zIndex = z + 0.0001f;
            itemProps.color = hovered
                                  ? m_style.hoverColor
                                  : Core::Renderer::Color{0.f, 0.f, 0.f, 0.f};
            itemProps.id = id;
            itemProps.transformMode = state.transformMode;
            state.renderer->drawQuad(itemProps);

            const auto textOffsetY = state.renderer->textCenterOffsetY(
                item.label,
                {
                    .fontSize = m_style.textStyle.fontSize,
                });
            const auto textPos = glm::vec2{
                m_menuPos.x + contentInset + m_style.metrics.padding.left,
                itemCenter.y + textOffsetY,
            };
            state.renderer->drawFont(
                item.label,
                {
                    .position = textPos,
                    .fontSize = m_style.textStyle.fontSize,
                    .color =
                        item.enabled ? textColor : disabledTextColor(textColor),
                    .zIndex = z + 0.0002f,
                    .id = id,
                    .transformMode = state.transformMode,
                });
            rowTop += rowHeight;
        }
    }
} // namespace Bess::Canvas::UI
