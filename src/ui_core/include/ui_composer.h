#pragma once

#include "controls/basic_widgets.h"
#include "controls/dock_space.h"
#include "controls/focus_scope.h"
#include "controls/menu_bar.h"
#include "controls/popup_controls.h"
#include "controls/scroll_view.h"
#include "controls/tab_bar.h"
#include "controls/text_box.h"
#include "controls/value_controls.h"
#include "widget_ref.h"

#include <concepts>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace Bess::UI {

    class UIComposer;

    struct DockPanelPlacement {
        DockNodeId target;
        DockZone zone = DockZone::main;
        bool closable = true;
        FlexContainerOptions content{
            .direction = LayoutDirection::vertical,
            .mainAxisAlignment = LayoutAlignment::start,
            .crossAxisAlignment = LayoutAlignment::start,
            .padding = Core::Style::Padding{12.f},
            .stretchWidth = true,
            .stretchHeight = true,
            .clipChildren = true,
            .hitTestVisible = false,
        };
    };

    // Focused facade for building DockSpace topology while panel content is
    // still composed from ordinary widgets. Docking identity never leaks into
    // the generic UIComposer API.
    class BESS_API DockComposer {
      public:
        DockComposer(WidgetTree &tree, WidgetRef<DockSpace> dockSpace);

        [[nodiscard]] WidgetRef<DockSpace> widget() const noexcept;
        [[nodiscard]] DockSpaceModel &model();
        [[nodiscard]] const DockSpaceModel &model() const;
        [[nodiscard]] DockNodeId stackFor(DockItemId item) const noexcept;

        DockPanelHandle panel(std::string title,
                              DockPanelPlacement placement = {});

        template <typename Build>
            requires std::invocable<Build, UIComposer &>
        DockPanelHandle
        panel(std::string title, DockPanelPlacement placement, Build &&build);

        template <typename Build>
            requires std::invocable<Build, UIComposer &>
        DockPanelHandle panel(std::string title, Build &&build) {
            return panel(std::move(title),
                         DockPanelPlacement{},
                         std::forward<Build>(build));
        }

      private:
        [[nodiscard]] WidgetId
        contentRoot(const DockPanelHandle &panel) const noexcept;
        void rollback(const DockPanelHandle &panel) noexcept;

        WidgetTree &m_tree;
        WidgetRef<DockSpace> m_dockSpace;
    };

    // Short-lived, stack-only authoring context. It adds widgets beneath one
    // parent, but WidgetTree remains the sole owner. Nested builder failures
    // roll back the subtree they started, so a failed composition cannot leave
    // half a control in the retained tree.
    class BESS_API UIComposer {
      public:
        explicit UIComposer(WidgetTree &tree, WidgetId parent = {});

        UIComposer(const UIComposer &) = delete;
        UIComposer &operator=(const UIComposer &) = delete;
        UIComposer(UIComposer &&) = delete;
        UIComposer &operator=(UIComposer &&) = delete;

        [[nodiscard]] WidgetTree &tree() const noexcept;
        [[nodiscard]] WidgetId parent() const noexcept;

        template <typename T, typename... Args>
            requires std::derived_from<T, Widget>
        WidgetRef<T> emplace(Args &&...args) {
            if (m_parent && !m_tree.contains(m_parent)) {
                throw std::logic_error(
                    "UIComposer parent is no longer in its WidgetTree");
            }
            const WidgetId id = m_tree.addWidget(
                std::make_unique<T>(std::forward<Args>(args)...), m_parent);
            if (!id) {
                throw std::runtime_error("Failed to compose a widget");
            }
            return WidgetRef<T>{m_tree, id};
        }

        WidgetRef<FlexContainer> row(FlexContainerOptions options = {});
        WidgetRef<FlexContainer> column(FlexContainerOptions options = {});
        WidgetRef<StackContainer> stack(StackContainerOptions options = {});
        WidgetRef<FocusScope> focusScope(FocusScopeOptions options = {});
        WidgetRef<Surface> surface(SurfaceOptions options = {});
        WidgetRef<Label> label(std::string text, LabelOptions options = {});
        WidgetRef<Button> button(std::string label,
                                 Button::Activated activated = {},
                                 ButtonOptions options = {});
        WidgetRef<CheckBox>
        checkBox(std::string label,
                 std::shared_ptr<CheckStateModel> model = {},
                 CheckBox::Changed changed = {},
                 CheckBoxOptions options = {});
        WidgetRef<ToggleSwitch> toggle(std::string label = {},
                                       std::shared_ptr<BoolModel> model = {},
                                       ToggleSwitch::Changed changed = {},
                                       ToggleSwitchOptions options = {});
        WidgetRef<RadioButton> radio(std::string label,
                                     std::shared_ptr<RadioGroupModel> group,
                                     RadioId id = {},
                                     RadioButton::Selected selected = {},
                                     RadioButtonOptions options = {});
        WidgetRef<Slider> slider(std::shared_ptr<RangeModel> model = {},
                                 Slider::Changed changed = {},
                                 SliderOptions options = {});
        WidgetRef<Dropdown> dropdown(std::shared_ptr<DropdownModel> model,
                                     Dropdown::Changed changed = {},
                                     DropdownOptions options = {});
        WidgetRef<TextBox> textBox(std::shared_ptr<TextEditModel> model = {},
                                   TextBox::Changed changed = {},
                                   TextBox::Submitted submitted = {},
                                   TextBoxOptions options = {});
        WidgetRef<Autocomplete>
        autocomplete(std::shared_ptr<TextEditModel> model,
                     AutocompleteProvider provider,
                     Autocomplete::Changed changed = {},
                     Autocomplete::Submitted submitted = {},
                     Autocomplete::Completed completed = {},
                     AutocompleteOptions options = {});
        WidgetRef<Tooltip> tooltip(std::string text,
                                   TooltipOptions options = {});
        WidgetRef<ContextMenuRegion>
        contextMenu(std::shared_ptr<MenuModel> model,
                    MenuId menu,
                    ContextMenuOptions options = {});
        WidgetRef<Spacer> spacer(SpacerOptions options = {});
        WidgetRef<Gap> gap(float pixels);
        WidgetRef<ScrollView> scrollView(ScrollViewOptions options = {});
        WidgetRef<TabBar> tabBar(std::shared_ptr<TabModel> model,
                                 TabBarOptions options = {});
        WidgetRef<DockSpace> dockSpace(DockSpaceOptions options = {});
        WidgetRef<MenuBar> menuBar(std::shared_ptr<MenuModel> model,
                                   MenuBarOptions options = {});

        template <typename Build>
            requires std::invocable<Build, UIComposer &>
        WidgetRef<FlexContainer> row(FlexContainerOptions options,
                                     Build &&build) {
            return composeChildren(row(std::move(options)),
                                   std::forward<Build>(build));
        }

        template <typename Build>
            requires std::invocable<Build, UIComposer &>
        WidgetRef<FlexContainer> row(Build &&build) {
            return row(FlexContainerOptions{}, std::forward<Build>(build));
        }

        template <typename Build>
            requires std::invocable<Build, UIComposer &>
        WidgetRef<FlexContainer> column(FlexContainerOptions options,
                                        Build &&build) {
            return composeChildren(column(std::move(options)),
                                   std::forward<Build>(build));
        }

        template <typename Build>
            requires std::invocable<Build, UIComposer &>
        WidgetRef<FlexContainer> column(Build &&build) {
            return column(FlexContainerOptions{}, std::forward<Build>(build));
        }

        template <typename Build>
            requires std::invocable<Build, UIComposer &>
        WidgetRef<StackContainer> stack(StackContainerOptions options,
                                        Build &&build) {
            return composeChildren(stack(std::move(options)),
                                   std::forward<Build>(build));
        }

        template <typename Build>
            requires std::invocable<Build, UIComposer &>
        WidgetRef<StackContainer> stack(Build &&build) {
            return stack(StackContainerOptions{}, std::forward<Build>(build));
        }

        template <typename Build>
            requires std::invocable<Build, UIComposer &>
        WidgetRef<FocusScope> focusScope(FocusScopeOptions options,
                                         Build &&build) {
            return composeChildren(focusScope(std::move(options)),
                                   std::forward<Build>(build));
        }

        template <typename Build>
            requires std::invocable<Build, UIComposer &>
        WidgetRef<FocusScope> focusScope(Build &&build) {
            return focusScope(FocusScopeOptions{}, std::forward<Build>(build));
        }

        template <typename Build>
            requires std::invocable<Build, UIComposer &>
        WidgetRef<Surface> surface(SurfaceOptions options, Build &&build) {
            return composeChildren(surface(std::move(options)),
                                   std::forward<Build>(build));
        }

        template <typename Build>
            requires std::invocable<Build, UIComposer &>
        WidgetRef<Surface> surface(Build &&build) {
            return surface(SurfaceOptions{}, std::forward<Build>(build));
        }

        template <typename Build>
            requires std::invocable<Build, UIComposer &>
        WidgetRef<ScrollView> scrollView(ScrollViewOptions options,
                                         Build &&build) {
            return composeSingleChild(scrollView(std::move(options)),
                                      std::forward<Build>(build),
                                      "ScrollView");
        }

        template <typename Build>
            requires std::invocable<Build, UIComposer &>
        WidgetRef<ScrollView> scrollView(Build &&build) {
            return scrollView(ScrollViewOptions{}, std::forward<Build>(build));
        }

        template <typename Build>
            requires std::invocable<Build, UIComposer &>
        WidgetRef<Tooltip>
        tooltip(std::string text, TooltipOptions options, Build &&build) {
            return composeSingleChild(
                tooltip(std::move(text), std::move(options)),
                std::forward<Build>(build),
                "Tooltip");
        }

        template <typename Build>
            requires std::invocable<Build, UIComposer &>
        WidgetRef<Tooltip> tooltip(std::string text, Build &&build) {
            return tooltip(
                std::move(text), TooltipOptions{}, std::forward<Build>(build));
        }

        template <typename Build>
            requires std::invocable<Build, UIComposer &>
        WidgetRef<ContextMenuRegion>
        contextMenu(std::shared_ptr<MenuModel> model,
                    MenuId menu,
                    ContextMenuOptions options,
                    Build &&build) {
            return composeSingleChild(
                contextMenu(std::move(model), menu, std::move(options)),
                std::forward<Build>(build),
                "ContextMenuRegion");
        }

        template <typename Build>
            requires std::invocable<Build, UIComposer &>
        WidgetRef<ContextMenuRegion> contextMenu(
            std::shared_ptr<MenuModel> model, MenuId menu, Build &&build) {
            return contextMenu(std::move(model),
                               menu,
                               ContextMenuOptions{},
                               std::forward<Build>(build));
        }

        template <typename Build>
            requires std::invocable<Build, DockComposer &>
        WidgetRef<DockSpace> dockSpace(DockSpaceOptions options,
                                       Build &&build) {
            auto result = dockSpace(std::move(options));
            try {
                DockComposer dock{m_tree, result};
                std::invoke(std::forward<Build>(build), dock);
            } catch (...) {
                result.remove();
                throw;
            }
            return result;
        }

        template <typename Build>
            requires std::invocable<Build, DockComposer &>
        WidgetRef<DockSpace> dockSpace(Build &&build) {
            return dockSpace(DockSpaceOptions{}, std::forward<Build>(build));
        }

        template <typename T, typename Configure>
            requires std::derived_from<T, Widget> &&
                     std::invocable<Configure, LayoutNode &>
        bool layout(const WidgetRef<T> &widget, Configure &&configure) const {
            return widget.tree() == &m_tree &&
                   widget.updateLayout(std::forward<Configure>(configure));
        }

        template <typename T>
            requires std::derived_from<T, Widget>
        bool layout(const WidgetRef<T> &widget, const LayoutSpec &spec) const {
            return widget.tree() == &m_tree && widget.setLayout(spec);
        }

        template <typename T>
            requires std::derived_from<T, Widget>
        bool enabled(const WidgetRef<T> &widget, bool value) const {
            return widget.tree() == &m_tree &&
                   m_tree.setEnabled(widget.id(), value);
        }

        template <typename T>
            requires std::derived_from<T, Widget>
        bool visibility(const WidgetRef<T> &widget,
                        WidgetVisibility value) const {
            return widget.tree() == &m_tree &&
                   m_tree.setVisibility(widget.id(), value);
        }

      private:
        template <typename T, typename Build>
            requires std::derived_from<T, Widget> &&
                     std::invocable<Build, UIComposer &>
        WidgetRef<T> composeChildren(WidgetRef<T> owner, Build &&build) {
            try {
                UIComposer children{m_tree, owner.id()};
                std::invoke(std::forward<Build>(build), children);
            } catch (...) {
                owner.remove();
                throw;
            }
            return owner;
        }

        template <typename T, typename Build>
            requires std::derived_from<T, Widget> &&
                     std::invocable<Build, UIComposer &>
        WidgetRef<T> composeSingleChild(WidgetRef<T> owner,
                                        Build &&build,
                                        std::string_view ownerName) {
            owner =
                composeChildren(std::move(owner), std::forward<Build>(build));
            if (m_tree.getChildren(owner.id()).size() <= 1) {
                return owner;
            }
            owner.remove();
            throw std::logic_error(std::string{ownerName} +
                                   " accepts at most one content root");
        }

        WidgetTree &m_tree;
        WidgetId m_parent;
    };

    template <typename Build>
        requires std::invocable<Build, UIComposer &>
    DockPanelHandle DockComposer::panel(std::string title,
                                        DockPanelPlacement placement,
                                        Build &&build) {
        const auto result = panel(std::move(title), std::move(placement));
        if (!result) {
            throw std::runtime_error("Failed to compose a dock panel");
        }
        const WidgetId content = contentRoot(result);
        if (!content) {
            rollback(result);
            throw std::runtime_error("Dock panel has no content root");
        }
        try {
            UIComposer composer{m_tree, content};
            std::invoke(std::forward<Build>(build), composer);
        } catch (...) {
            rollback(result);
            throw;
        }
        return result;
    }

} // namespace Bess::UI
