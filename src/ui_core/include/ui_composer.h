#pragma once

#include "controls/basic_widgets.h"
#include "controls/dock_space.h"
#include "controls/menu_bar.h"
#include "controls/tab_bar.h"
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
        WidgetRef<Surface> surface(SurfaceOptions options = {});
        WidgetRef<Label> label(std::string text, LabelOptions options = {});
        WidgetRef<Button> button(std::string label,
                                 Button::Activated activated = {},
                                 ButtonOptions options = {});
        WidgetRef<Spacer> spacer();
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
