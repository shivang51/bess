#pragma once

#include "bess_core/scene/scene_ui/ui_scene_component.h"
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Bess::Canvas::UI {

    struct UIDropdownOption {
        std::string label;
        bool enabled = true;
    };

    using UIDropdownCallback =
        std::function<void(size_t, const UIDropdownOption &)>;

    class DropdownComp : public UISceneComponent {
      public:
        DEFAULT_CONTRS(DropdownComp)

        MAKE_GETTER_SETTER_WC(std::string,
                              Placeholder,
                              m_placeholder,
                              makeUIDirty)
        MAKE_GETTER_SETTER_WC(glm::vec2, HeaderSize, m_headerSize, makeUIDirty)
        MAKE_GETTER_SETTER_WC(float, ItemHeight, m_itemHeight, makeUIDirty)
        MAKE_GETTER_SETTER_WC(float, MenuWidth, m_menuWidth, makeUIDirty)
        MAKE_GETTER_SETTER_WC(size_t,
                              MaxVisibleItems,
                              m_maxVisibleItems,
                              makeUIDirty)
        MAKE_GETTER_SETTER(UIDropdownCallback,
                           ChangedCallback,
                           m_changedCallback)
        MAKE_GETTER(bool, Open, m_open)

        static std::shared_ptr<DropdownComp>
        create(const CompConfig &config);
        static std::shared_ptr<DropdownComp>
        create(const std::vector<UIDropdownOption> &options = {},
               size_t selectedIndex = 0,
               const UIDropdownCallback &changedCallback = nullptr,
               const CompConfig &config = CompConfig{});

        void setOptions(const std::vector<UIDropdownOption> &options);
        const std::vector<UIDropdownOption> &getOptions() const;
        [[nodiscard]] size_t getSelectedIndex() const;
        void setSelectedIndex(size_t index);
        void open();
        void close();
        void toggleOpen();

        void onDraw(SceneDrawContext &state) override;
        void prepareUI(SceneUIPrepareCtx &state) override;
        bool onMouseEnter(const Events::MouseEnterEvent &e) override;
        bool onMouseLeave(const Events::MouseLeaveEvent &e) override;
        bool onMouseButton(const Events::MouseButtonEvent &e) override;
        bool onMouseWheel(const Events::MouseWheelEvent &e) override;
        bool isFocusable() const override;
        bool wantsKeyboardInput() const override;
        void onFocusLost(const Events::FocusEvent &e) override;
        bool onKeyEvent(const SceneEvent &evt) override;

      private:
        [[nodiscard]] std::string selectedLabel() const;
        [[nodiscard]] size_t validSelectedIndex(size_t index) const;
        [[nodiscard]] bool isOptionInfo(uint32_t info) const;
        [[nodiscard]] size_t optionIndexFromInfo(uint32_t info) const;
        [[nodiscard]] glm::vec2 resolveHeaderSize(SceneUIPrepareCtx &state);
        [[nodiscard]] float resolveMenuWidth() const;

        void selectFromUser(size_t index);
        void ensureSelectedVisible();
        void drawChevron(SceneDrawContext &state, const UINode *node);
        void drawMenu(SceneDrawContext &state);

        std::vector<UIDropdownOption> m_options;
        size_t m_selectedIndex = 0;
        bool m_open = false;
        std::string m_placeholder = "Select";
        glm::vec2 m_headerSize{120.f, 0.f};
        float m_itemHeight = 20.f;
        float m_menuWidth = 0.f;
        size_t m_maxVisibleItems = 8;
        UINode *m_labelNode = nullptr;
        UINode *m_chevronNode = nullptr;
        UIDropdownCallback m_changedCallback;
        glm::vec2 m_cachedHeaderSize{0.f};
        float m_cachedMenuWidth = 0.f;
        uint32_t m_hoveredInfo = 0u;
        size_t m_scrollOffset = 0;
    };
} // namespace Bess::Canvas::UI
