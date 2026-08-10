#pragma once

#include "common/bess_api.h"

#include "bess_core/scene/scene_ui/ui_scene_component.h"
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Bess::Canvas::UI {

    struct BESS_API UIContextMenuItem {
        std::string label;
        std::function<void()> callback = nullptr;
        bool enabled = true;
        bool separator = false;
    };

    class BESS_API ContextMenuComp : public UISceneComponent {
      public:
        DEFAULT_CONTRS(ContextMenuComp)

        MAKE_GETTER_SETTER_WC(std::string,
                              TriggerLabel,
                              m_triggerLabel,
                              makeUIDirty)
        MAKE_GETTER_SETTER_WC(bool, ShowTrigger, m_showTrigger, makeUIDirty)
        MAKE_GETTER_SETTER_WC(glm::vec2,
                              TriggerSize,
                              m_triggerSize,
                              makeUIDirty)
        MAKE_GETTER_SETTER_WC(float, MenuWidth, m_menuWidth, makeUIDirty)
        MAKE_GETTER_SETTER_WC(float, ItemHeight, m_itemHeight, makeUIDirty)
        MAKE_GETTER(bool, Open, m_open)

        static std::shared_ptr<ContextMenuComp>
        create(const CompConfig &config);
        static std::shared_ptr<ContextMenuComp>
        create(const std::vector<UIContextMenuItem> &items = {},
               const std::string &triggerLabel = "Right click",
               const CompConfig &config = CompConfig{});

        void setItems(const std::vector<UIContextMenuItem> &items);
        const std::vector<UIContextMenuItem> &getItems() const;
        void showAt(const glm::vec2 &position);
        void hide();

        void onDraw(SceneDrawContext &state) override;
        void prepareUI(SceneUIPrepareCtx &state) override;
        bool onMouseEnter(const Events::MouseEnterEvent &e) override;
        bool onMouseLeave(const Events::MouseLeaveEvent &e) override;
        bool onMouseButton(const Events::MouseButtonEvent &e) override;
        bool isFocusable() const override;
        bool wantsKeyboardInput() const override;
        void onFocusLost(const Events::FocusEvent &e) override;
        bool onKeyEvent(const SceneEvent &evt) override;

      private:
        [[nodiscard]] bool isItemInfo(uint32_t info) const;
        [[nodiscard]] size_t itemIndexFromInfo(uint32_t info) const;
        [[nodiscard]] float resolveMenuWidth(SceneUIPrepareCtx &state) const;

        void activateItem(size_t index);
        void drawMenu(SceneDrawContext &state);

        std::vector<UIContextMenuItem> m_items;
        std::string m_triggerLabel = "Right click";
        bool m_showTrigger = true;
        bool m_open = false;
        glm::vec2 m_triggerSize{130.f, 24.f};
        float m_menuWidth = 0.f;
        float m_itemHeight = 20.f;
        glm::vec2 m_menuPos{0.f};
        UINode *m_labelNode = nullptr;
        float m_cachedMenuWidth = 0.f;
        uint32_t m_hoveredInfo = 0u;
    };
} // namespace Bess::Canvas::UI
