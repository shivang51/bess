#pragma once

#include "bess_core/scene/scene_ui/ui_scene_component.h"
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Bess::Canvas::UI {

    struct UIListBoxItem {
        std::string label;
        bool enabled = true;
    };

    using UIListBoxCallback =
        std::function<void(size_t, const UIListBoxItem &)>;

    class ListBoxComp : public UISceneComponent {
      public:
        DEFAULT_CONTRS(ListBoxComp)

        static constexpr size_t noSelection = static_cast<size_t>(-1);

        static std::shared_ptr<ListBoxComp>
        create(const CompConfig &config);
        static std::shared_ptr<ListBoxComp>
        create(const std::vector<UIListBoxItem> &items = {},
               size_t selectedIndex = noSelection,
               const UIListBoxCallback &changedCallback = nullptr,
               const CompConfig &config = CompConfig{});

        void setItems(const std::vector<UIListBoxItem> &items);
        const std::vector<UIListBoxItem> &getItems() const;

        [[nodiscard]] std::optional<size_t> getSelectedIndex() const;
        [[nodiscard]] bool hasWidgetChildren() const;
        void setSelectedIndex(size_t index);
        void clearSelection();

        MAKE_GETTER_SETTER_WC(glm::vec2, ListSize, m_listSize, makeUIDirty)
        MAKE_GETTER_SETTER_WC(float, ItemHeight, m_itemHeight, makeUIDirty)
        MAKE_GETTER_SETTER_WC(float,
                              ScrollbarWidth,
                              m_scrollbarWidth,
                              makeUIDirty)
        MAKE_GETTER_SETTER_WC(float,
                              MinThumbHeight,
                              m_minThumbHeight,
                              makeUIDirty)
        MAKE_GETTER_SETTER(float, WheelScrollRows, m_wheelScrollRows)
        MAKE_GETTER_SETTER_WC(bool, ShowScrollbar, m_showScrollbar, makeUIDirty)
        MAKE_GETTER_SETTER(UIListBoxCallback,
                           ChangedCallback,
                           m_changedCallback)
        MAKE_GETTER_SETTER_WC(LayoutAlignment,
                              ChildAlignment,
                              m_childAlignment,
                              makeUIDirty)

        void scrollToIndex(size_t index);
        void scrollToSelection();

        std::vector<UUID> cleanup(SceneState &state,
                                  UUID caller = UUID::null) override;
        void onDraw(SceneDrawContext &state) override;
        void prepareUI(SceneUIPrepareCtx &state) override;
        bool onMouseEnter(const Events::MouseEnterEvent &e) override;
        bool onMouseLeave(const Events::MouseLeaveEvent &e) override;
        bool onMouseButton(const Events::MouseButtonEvent &e) override;
        bool onMouseWheel(const Events::MouseWheelEvent &e) override;
        bool onPointerMove(const Events::MouseMoveEvent &e) override;
        bool hasPointerCapture() const override;
        bool isFocusable() const override;
        bool wantsKeyboardInput() const override;
        bool onKeyEvent(const SceneEvent &evt) override;
        Core::Viewport::SceneCursor getCursor() const override;

        struct Rect {
            float left = 0.f;
            float top = 0.f;
            float right = 0.f;
            float bottom = 0.f;
        };

        struct VisibleRange {
            size_t first = 0;
            size_t count = 0;
            float firstTop = 0.f;
        };

      private:
        void prepStyle(
            const std::shared_ptr<Core::Style::BessTheme> &theme) override;
        void selectFromUser(size_t index);
        void setScrollOffset(float offset);
        void scrollBy(float delta);
        void clampScrollOffset();
        void ensureIndexVisible(size_t index);
        void updateScrollFromThumbDrag(float pointerY);
        void drawBackground(SceneDrawContext &state);
        void drawItems(SceneDrawContext &state, const Rect &contentRect);
        void drawChildren(SceneDrawContext &state, const Rect &contentRect);
        void drawScrollbar(SceneDrawContext &state,
                           const Rect &contentRect,
                           const Rect &scrollbarRect);
        void initContentNode(const std::shared_ptr<UINodeRegistry> &registry);
        void prepareChildren(SceneUIPrepareCtx &state);
        void configureContentNode();

        [[nodiscard]] glm::vec2 resolveListSize(SceneUIPrepareCtx &state) const;
        [[nodiscard]] Rect nodeRect() const;
        [[nodiscard]] Rect nodeRect(const UINode *node) const;
        [[nodiscard]] Rect contentRect() const;
        [[nodiscard]] Rect localContentRect() const;
        [[nodiscard]] Rect scrollableContentRect() const;
        [[nodiscard]] Rect scrollbarRect(const Rect &contentRect) const;
        [[nodiscard]] float itemHeight() const;
        [[nodiscard]] float totalContentHeight() const;
        [[nodiscard]] float maxScrollOffset() const;
        [[nodiscard]] bool hasScrollableContent(const Rect &contentRect) const;
        [[nodiscard]] bool reserveScrollbarSpace(const Rect &contentRect) const;
        [[nodiscard]] VisibleRange visibleRange(const Rect &contentRect) const;
        [[nodiscard]] bool intersects(const Rect &lhs, const Rect &rhs) const;
        [[nodiscard]] bool isItemInfo(uint32_t info) const;
        [[nodiscard]] size_t itemIndexFromInfo(uint32_t info) const;
        [[nodiscard]] size_t nextEnabledIndex(size_t start) const;
        [[nodiscard]] size_t previousEnabledIndex(size_t start) const;
        [[nodiscard]] bool pushClip(SceneDrawContext &state,
                                    const Rect &rect) const;
        void onChildrenChanged() override;

        std::vector<UIListBoxItem> m_items;
        std::optional<size_t> m_selectedIndex;
        UIListBoxCallback m_changedCallback;
        glm::vec2 m_listSize{150.f, 112.f};
        float m_itemHeight = 20.f;
        float m_scrollbarWidth = 8.f;
        float m_minThumbHeight = 18.f;
        float m_wheelScrollRows = 3.f;
        bool m_showScrollbar = true;
        LayoutAlignment m_childAlignment = LayoutAlignment::start;
        bool m_draggingThumb = false;
        float m_dragStartPointerY = 0.f;
        float m_dragStartScrollOffset = 0.f;
        float m_scrollOffset = 0.f;
        glm::vec2 m_cachedListSize{150.f, 112.f};
        UINode *m_contentNode = nullptr;
        uint32_t m_hoveredInfo = 0u;
        Color m_selectedTextColor{1.f, 1.f, 1.f, 1.f};
        Color m_disabledTextColor{0.5f, 0.5f, 0.5f, 1.f};
        Color m_selectedRowColor{0.f, 0.f, 0.f, 0.f};
        Color m_scrollbarTrackColor{0.f, 0.f, 0.f, 0.f};
        Color m_scrollbarThumbColor{0.f, 0.f, 0.f, 0.f};
        Color m_scrollbarThumbHoverColor{0.f, 0.f, 0.f, 0.f};
    };

} // namespace Bess::Canvas::UI
