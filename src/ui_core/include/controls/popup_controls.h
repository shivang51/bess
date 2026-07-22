#pragma once

#include "behaviors/pressable.h"
#include "controls/text_box.h"
#include "models/dropdown_model.h"
#include "models/menu_model.h"
#include "popup.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Bess::UI {

    struct AutocompleteItem {
        AutocompleteItemId id = AutocompleteItemId::generate();
        std::string label;
        std::string replacement;
        std::string detail;
        std::string icon;
        bool enabled = true;
    };

    using AutocompleteProvider =
        std::function<std::vector<AutocompleteItem>(std::string_view)>;

    struct AutocompleteOptions {
        TextBoxOptions textBox;
        std::optional<UIDropdownStyle> popupStyle;
        size_t minimumCharacters = 1;
        size_t maximumItems = 12;
        bool selectFirst = true;
        float popupMaximumHeight = 280.f;
    };

    namespace Detail {
        struct AutocompleteSession;
    }

    class BESS_API Autocomplete : public Widget {
      public:
        using Changed = TextBox::Changed;
        using Submitted = TextBox::Submitted;
        using Completed = std::function<void(const AutocompleteItem &)>;

        Autocomplete(std::shared_ptr<TextEditModel> model,
                     AutocompleteProvider provider,
                     Changed changed = {},
                     Submitted submitted = {},
                     Completed completed = {},
                     AutocompleteOptions options = {});

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;
        void onUnmount(WidgetTree &state, WidgetId id) override;
        void update(WidgetUpdateContext &context) override;
        void arrange(WidgetArrangeContext &context) override;
        UIEventReply onEvent(WidgetEventContext &context,
                             const UIEvent &event) override;

        [[nodiscard]] std::shared_ptr<TextEditModel> model() const noexcept;
        [[nodiscard]] WidgetId textBoxId() const noexcept;
        void setProvider(AutocompleteProvider provider);
        void refresh();
        bool close();
        [[nodiscard]] bool isOpen() const noexcept;

      private:
        void open();
        void complete(size_t index);
        void moveSelection(int direction);
        [[nodiscard]] size_t
        graphemeCount(std::string_view text) const noexcept;

        std::shared_ptr<TextEditModel> m_model;
        AutocompleteProvider m_provider;
        Changed m_changed;
        Submitted m_submitted;
        Completed m_completed;
        AutocompleteOptions m_options;
        std::shared_ptr<Detail::AutocompleteSession> m_session;
        TextEditModel::ChangedSignal::Connection m_connection;
        PopupHandle m_popup;
        WidgetTree *m_state = nullptr;
        WidgetId m_id;
        WidgetId m_textBox;
        std::string m_dismissedQuery;
        bool m_wasFocused = false;
        bool m_popupWasOpen = false;
        bool m_suppressRefresh = false;
    };

    struct DropdownOptions {
        std::optional<UIDropdownStyle> style;
        std::string placeholder = "Select";
        bool autoSize = true;
    };

    class BESS_API Dropdown : public Widget {
      public:
        using Changed = std::function<void(DropdownItemId)>;

        Dropdown(std::shared_ptr<DropdownModel> model,
                 Changed changed = {},
                 DropdownOptions options = {});

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;
        void onUnmount(WidgetTree &state, WidgetId id) override;
        void updateLayout(WidgetLayoutContext &context) override;
        void paint(WidgetPaintContext &context) const override;
        [[nodiscard]] CursorIcon
        cursor(const WidgetCursorContext &context) const noexcept override;
        UIEventReply onEvent(WidgetEventContext &context,
                             const UIEvent &event) override;

        [[nodiscard]] std::shared_ptr<DropdownModel> model() const noexcept;
        [[nodiscard]] DropdownItemId selection() const noexcept;
        bool select(DropdownItemId item);
        [[nodiscard]] bool isOpen() const noexcept;
        bool open();
        bool close();

      private:
        void reconnectModel();

        std::shared_ptr<DropdownModel> m_model;
        Changed m_changed;
        DropdownOptions m_options;
        DropdownModel::ChangedSignal::Connection m_connection;
        Pressable m_pressable;
        PopupHandle m_popup;
        WidgetTree *m_state = nullptr;
        WidgetId m_id;
        bool m_intrinsicSizeDirty = true;
    };

    struct ContextMenuOptions {
        std::optional<UIMenuStyle> style;
        float maximumHeight = 520.f;
        AnchoredPopupOptions popup{
            .preferredSide = PopupSide::bottom,
            .alignment = PopupAlignment::start,
            .gap = 0.f,
            .dismissOnOutsidePress = true,
            .dismissOnEscape = true,
            .closeWhenAnchorGone = true,
            .interactive = true,
            .focus = {.trapFocus = true,
                      .autoFocus = true,
                      .restoreFocus = true},
            .content = {.direction = LayoutDirection::vertical,
                        .mainAxisAlignment = LayoutAlignment::start,
                        .crossAxisAlignment = LayoutAlignment::start,
                        .stretchWidth = false,
                        .stretchHeight = false,
                        .clipChildren = false,
                        .hitTestVisible = false},
        };
    };

    // Context menus are popup sessions, not permanent widgets. The model is
    // shared with MenuBar so command state and nested submenus stay uniform.
    class BESS_API ContextMenu {
      public:
        [[nodiscard]] static PopupHandle open(PopupHost &host,
                                              std::shared_ptr<MenuModel> model,
                                              MenuId menu,
                                              PopupAnchor anchor,
                                              ContextMenuOptions options = {});

        [[nodiscard]] static PopupHandle open(PopupHost &host,
                                              std::vector<MenuItem> items,
                                              glm::vec2 position,
                                              ContextMenuOptions options = {});
    };

    class BESS_API ContextMenuRegion : public Widget {
      public:
        ContextMenuRegion(std::shared_ptr<MenuModel> model,
                          MenuId menu,
                          ContextMenuOptions options = {});

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;
        void onUnmount(WidgetTree &state, WidgetId id) override;
        void arrange(WidgetArrangeContext &context) override;
        UIEventReply onEvent(WidgetEventContext &context,
                             const UIEvent &event) override;

        [[nodiscard]] bool isOpen() const noexcept;
        bool close();

      private:
        std::shared_ptr<MenuModel> m_model;
        MenuId m_menu;
        ContextMenuOptions m_options;
        PopupHandle m_popup;
        WidgetTree *m_state = nullptr;
        WidgetId m_id;
    };

    struct TooltipOptions {
        std::optional<UITooltipStyle> style;
        PopupSide side = PopupSide::top;
        PopupAlignment alignment = PopupAlignment::center;
        std::optional<float> delayMs;
        float gap = 6.f;
    };

    // Transparent single-child wrapper. It observes the tree's current pointer
    // position in update(), so the wrapped control keeps its normal hit testing
    // and event behavior.
    class BESS_API Tooltip : public Widget {
      public:
        Tooltip(std::string text, TooltipOptions options = {});

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;
        void onUnmount(WidgetTree &state, WidgetId id) override;
        void update(WidgetUpdateContext &context) override;
        void arrange(WidgetArrangeContext &context) override;

        [[nodiscard]] const std::string &text() const noexcept;
        void setText(std::string text);
        bool showNow();
        bool hide();
        [[nodiscard]] bool isOpen() const noexcept;

      private:
        std::string m_text;
        TooltipOptions m_options;
        PopupHandle m_popup;
        WidgetTree *m_state = nullptr;
        WidgetId m_id;
        double m_hoverElapsedMs = 0.0;
        bool m_wasInside = false;
    };

} // namespace Bess::UI
