#include "widget.h"

#include "widget_tree.h"

namespace Bess::UI {
    std::span<const WidgetId> WidgetArrangeContext::children() const noexcept {
        return state.getChildren(id);
    }

    bool WidgetArrangeContext::isDirectChild(WidgetId child) const noexcept {
        return state.getParent(child) == id;
    }

    bool WidgetArrangeContext::setChildBounds(WidgetId child,
                                              WidgetBounds childBounds) {
        return state.setArrangedBounds(id, child, childBounds);
    }

    bool WidgetArrangeContext::setChildVisible(WidgetId child, bool visible) {
        return state.setArrangedVisible(id, child, visible);
    }

    Widget::~Widget() = default;

    std::string_view Widget::typeName() const noexcept {
        return "Widget";
    }

    WidgetTraits Widget::traits() const noexcept {
        return {};
    }

    void Widget::onMount(WidgetMountContext &) {
    }

    void Widget::onUnmount(WidgetTree &, WidgetId) {
    }

    void Widget::updateLayout(WidgetLayoutContext &) {
    }

    void Widget::update(WidgetUpdateContext &) {
    }

    void Widget::arrange(WidgetArrangeContext &) {
    }

    void Widget::paint(WidgetPaintContext &) const {
    }

    void Widget::paintOverlay(WidgetPaintContext &) const {
    }

    CursorIcon Widget::cursor(const WidgetCursorContext &) const noexcept {
        return CursorIcon::inherit;
    }

    bool Widget::hitTest(WidgetBounds bounds,
                         glm::vec2 position) const noexcept {
        return bounds.contains(position);
    }

    UIEventReply Widget::onEvent(WidgetEventContext &, const UIEvent &) {
        return {};
    }
} // namespace Bess::UI
