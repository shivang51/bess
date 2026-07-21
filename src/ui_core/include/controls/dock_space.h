#pragma once

#include "behaviors/pressable.h"
#include "models/dock_model.h"
#include "ui_style.h"
#include "widget.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace Bess::UI {

    struct DockPanelOptions {
        std::optional<UIBoxStyle> background;
        bool clipContent = true;
    };

    // Semantic wrapper for arbitrary dockable content. The DockSpace model
    // refers to this widget by WidgetId; its actual content remains an ordinary
    // child in WidgetTree and has no dependency on docking.
    class BESS_API DockPanel : public Widget {
      public:
        explicit DockPanel(std::string title, DockPanelOptions options = {});

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;
        void arrange(WidgetArrangeContext &context) override;
        void paint(WidgetPaintContext &context) const override;

        [[nodiscard]] DockItemId itemId() const noexcept;
        [[nodiscard]] const std::string &title() const noexcept;
        void setTitle(std::string title);

      private:
        DockItemId m_itemId = DockItemId::generate();
        std::string m_title;
        DockPanelOptions m_options;
    };

    struct DockPanelHandle {
        DockItemId item;
        WidgetId panel;

        [[nodiscard]] explicit operator bool() const noexcept {
            return item && panel;
        }
    };

    struct DockSpaceOptions {
        std::optional<UIDockStyle> dockStyle;
        std::optional<UITabStyle> tabStyle;
    };

    class BESS_API DockSpace : public Widget {
      public:
        explicit DockSpace(DockSpaceOptions options = {});

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;
        void onUnmount(WidgetTree &state, WidgetId id) override;
        void arrange(WidgetArrangeContext &context) override;
        void paint(WidgetPaintContext &context) const override;
        UIEventReply onEvent(WidgetEventContext &context,
                             const UIEvent &event) override;

        [[nodiscard]] DockSpaceModel &model() noexcept;
        [[nodiscard]] const DockSpaceModel &model() const noexcept;

        DockPanelHandle createPanel(WidgetTree &state,
                                    WidgetId dockSpace,
                                    std::string title,
                                    std::unique_ptr<Widget> content = {},
                                    DockNodeId target = {},
                                    DockZone zone = DockZone::main,
                                    bool closable = true);
        bool removePanel(WidgetTree &state, DockItemId item);
        bool
        setPanelTitle(WidgetTree &state, DockItemId item, std::string title);

      private:
        struct HitTab {
            DockNodeId stack;
            DockItemId item;
        };

        [[nodiscard]] const UIDockStyle &
        dockStyle(const WidgetTree &state) const;
        [[nodiscard]] const UITabStyle &tabStyle(const WidgetTree &state) const;
        [[nodiscard]] DockLayoutResult
        calculateLayout(WidgetBounds bounds, const WidgetTree &state) const;
        [[nodiscard]] HitTab hitTab(WidgetBounds bounds,
                                    const WidgetTree &state,
                                    glm::vec2 position) const;

        DockSpaceModel m_model;
        DockSpaceOptions m_options;
        Pressable m_tabPressable;
        DockItemId m_hoveredItem;
        DockItemId m_pressedItem;
        DockNodeId m_focusedStack;
        DockNodeId m_hoveredSplit;
        DockNodeId m_draggedSplit;
        DockSpaceModel::ChangedSignal::Connection m_modelConnection;
    };

} // namespace Bess::UI
