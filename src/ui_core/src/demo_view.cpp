#include "demo_view.h"
#include "bess_core/style/bess_theme.h"

#include <format>
#include <functional>
#include <utility>
#include <vector>

namespace Bess::UI {
    namespace {
        void stretch(LayoutNode &layout) {
            layout.setWidthStretch();
            layout.setHeightStretch();
        }

        FlexContainerOptions columnOptions(float padding = 0.f) {
            return {
                .direction = LayoutDirection::vertical,
                .mainAxisAlignment = LayoutAlignment::start,
                .crossAxisAlignment = LayoutAlignment::start,
                .padding = Core::Style::Padding{padding},
                .stretchWidth = true,
                .stretchHeight = true,
                .clipChildren = false,
                .hitTestVisible = false,
            };
        }

        FlexContainerOptions headerOptions() {
            return {
                .direction = LayoutDirection::horizontal,
                .mainAxisAlignment = LayoutAlignment::start,
                .crossAxisAlignment = LayoutAlignment::center,
                .padding = Core::Style::Padding::fromSymmetric(8.f, 7.f),
                .gap = 6.f,
                .stretchWidth = true,
                .stretchHeight = false,
                .clipChildren = false,
                .hitTestVisible = false,
            };
        }

        FlexContainerOptions menuHeaderOptions() {
            return {
                .direction = LayoutDirection::horizontal,
                .mainAxisAlignment = LayoutAlignment::start,
                .crossAxisAlignment = LayoutAlignment::center,
                .padding = Core::Style::Padding::fromHorizontal(4.f),
                .stretchWidth = true,
                .stretchHeight = false,
                .clipChildren = false,
                .hitTestVisible = false,
            };
        }

        LabelOptions applicationIconLabel() {
            return {.fontSize = 13.f};
        }

        LabelOptions headingLabel() {
            return {.fontSize = 18.f};
        }

        MenuItem command(std::string icon,
                         std::string name,
                         std::string shortcut,
                         std::function<void()> activated = {},
                         std::vector<MenuItem> children = {}) {
            return {.icon = std::move(icon),
                    .name = std::move(name),
                    .shortcut = std::move(shortcut),
                    .activated = std::move(activated),
                    .children = std::move(children)};
        }
    } // namespace

    void UIDemoView::compose(UIComposer &ui) {
        m_tabs = std::make_shared<TabModel>();
        static_cast<void>(m_tabs->add("Workspace"));
        static_cast<void>(m_tabs->add("Components"));
        static_cast<void>(m_tabs->add("Diagnostics"));

        m_menus = std::make_shared<MenuModel>();
        static_cast<void>(m_menus->addMenu({
            .name = "File",
            .items =
                {
                    command("+",
                            "New",
                            "Ctrl+N",
                            [this] { setStatus("File > New activated"); }),
                    command("O",
                            "Open...",
                            "Ctrl+O",
                            [this] { setStatus("File > Open activated"); }),
                    MenuItem::separator(),
                    command("",
                            "Open Recent",
                            "",
                            {},
                            {command("",
                                     "demo.bess",
                                     "",
                                     [this] {
                                         setStatus("Opened recent demo.bess");
                                     }),
                             command("",
                                     "architecture.bess",
                                     "",
                                     [this] {
                                         setStatus(
                                             "Opened recent architecture.bess");
                                     })}),
                    MenuItem::separator(),
                    command(
                        "",
                        "Exit",
                        "Alt+F4",
                        [this] {
                            setStatus(
                                "Exit command exercised (demo does not quit)");
                        }),
                },
        }));
        static_cast<void>(m_menus->addMenu({
            .name = "Edit",
            .items =
                {
                    command("",
                            "Undo",
                            "Ctrl+Z",
                            [this] { setStatus("Undo activated"); }),
                    command("",
                            "Redo",
                            "Ctrl+Shift+Z",
                            [this] { setStatus("Redo activated"); }),
                    MenuItem::separator(),
                    command("",
                            "Preferences",
                            "Ctrl+,",
                            [this] { setStatus("Preferences activated"); }),
                },
        }));
        static_cast<void>(m_menus->addMenu({
            .name = "View",
            .items =
                {
                    command("",
                            "Panels",
                            "",
                            {},
                            {command("",
                                     "Explorer",
                                     "Ctrl+1",
                                     [this] {
                                         if (m_explorerPanel.show()) {
                                             setStatus("Explorer shown");
                                         }
                                     }),
                             command("",
                                     "Inspector",
                                     "Ctrl+2",
                                     [this] {
                                         if (m_inspectorPanel.show()) {
                                             setStatus("Inspector shown");
                                         }
                                     }),
                             command("",
                                     "Console",
                                     "Ctrl+3",
                                     [this] {
                                         if (m_consolePanel.show()) {
                                             setStatus("Console shown");
                                         }
                                     }),
                             command("",
                                     "Preview",
                                     "",
                                     [this] {
                                         if (m_previewPanel.show()) {
                                             setStatus("Preview shown");
                                         }
                                     }),
                             command("",
                                     "Assets",
                                     "",
                                     [this] {
                                         if (m_assetsPanel.show()) {
                                             setStatus("Assets shown");
                                         }
                                     })}),
                    command("",
                            "Command Palette",
                            "Ctrl+Shift+P",
                            [this] { setStatus("Command Palette activated"); }),
                },
        }));
        static_cast<void>(m_menus->addMenu({
            .name = "Help",
            .items = {command(
                "?",
                "About Bess UI",
                "",
                [this] { setStatus("Bess UI Core retained-mode demo"); })},
        }));

        auto shell = ui.surface([this](UIComposer &surface) {
            auto page =
                surface.column(columnOptions(), [this](UIComposer &page) {
                    auto menuHeader = page.row(
                        menuHeaderOptions(), [this](UIComposer &header) {
                            header.label("B", applicationIconLabel());
                            header.gap(4.f);
                            m_menuBar = header.menuBar(m_menus);
                            header.spacer(
                                SpacerOptions{.minimumSize = {0.f, 26.f}});
                            auto action = header.button(
                                "?", [this] { setStatus("Toolbar action"); });
                            action.updateLayout([](LayoutNode &layout) {
                                layout.setMinSize({20.f, 20.f});
                                layout.setWidth(20.f);
                                layout.setHeight(20.f);
                            });
                        });
                    menuHeader.updateLayout(
                        [](LayoutNode &layout) { layout.setFlexShrink(0.f); });
                    auto header =
                        page.row(headerOptions(), [this](UIComposer &header) {
                            header.label("Bess UI Core", headingLabel());
                            m_status = header.label(
                                "Drag a dock tab to float it; use the node "
                                "guides to dock it again",
                                LabelOptions{.horizontal =
                                                 HorizontalTextAlignment::end,
                                             .autoSize = false});
                            m_status.updateLayout([](LayoutNode &layout) {
                                layout.setWidthAuto();
                                layout.setHeight(24.f);
                                layout.setMinSize({80.f, 24.f});
                                layout.setFlex(1.f, 1.f, 0.f);
                            });

                            header.button("Add tab", [this] {
                                m_tabs->add(
                                    std::format("New tab {}", m_tabs->size()));
                            });

                            header.button("Next tab",
                                          [this] { selectNextTab(); });
                            m_counterButton = header.button(
                                "Count: 0",
                                [this] { incrementCounter(); },
                                ButtonOptions{.autoSize = false});
                            m_counterButton.updateLayout(
                                [](LayoutNode &layout) {
                                    layout.setWidth(78.f);
                                    layout.setHeight(26.f);
                                });

                            auto disabled = header.button("Disabled");
                            static_cast<void>(header.enabled(disabled, false));
                        });
                    header.updateLayout(
                        [](LayoutNode &layout) { layout.setFlexShrink(0.f); });

                    m_tabBar = page.tabBar(m_tabs);
                    m_tabBar.updateLayout(
                        [](LayoutNode &layout) { layout.setFlexShrink(0.f); });

                    m_dockSpace = page.dockSpace([this](DockComposer &dock) {
                        m_explorerPanel =
                            dock.panel("Explorer", [this](UIComposer &panel) {
                                panel.label("Project Explorer", headingLabel());
                                panel.label("assets/");
                                panel.label("plugins/");
                                panel.label("src/");
                                panel.spacer();
                                panel.button("Create item", [this] {
                                    setStatus(
                                        "Explorer: create item activated");
                                });
                            });

                        // const auto explorer2 = dock.panel(
                        //     "Component Explorer", [this](UIComposer &panel) {
                        //         panel.label("Component Explorer",
                        //                     headingLabel());
                        //         panel.spacer();
                        //         panel.button("Select item", [this] {
                        //             setStatus("Comp selected");
                        //         });
                        //     });

                        m_inspectorPanel = dock.panel(
                            "Inspector",
                            DockPanelPlacement{
                                .target = dock.stackFor(m_explorerPanel.item),
                                .zone = DockZone::right,
                            },
                            [this](UIComposer &panel) {
                                panel.label("Inspector", headingLabel());
                                panel.label("Transform");
                                panel.label("Layout");
                                panel.spacer();
                                panel.button("Apply", [this] {
                                    setStatus("Inspector changes applied");
                                });
                            });

                        m_previewPanel = dock.panel(
                            "Preview",
                            DockPanelPlacement{
                                .target = dock.stackFor(m_inspectorPanel.item),
                                .zone = DockZone::main,
                            },
                            [](UIComposer &panel) {
                                panel.label("Preview", headingLabel());
                                panel.label(
                                    "This shares a tab stack with Inspector.");
                                panel.spacer();
                                panel.button("Refresh preview");
                            });

                        m_consolePanel = dock.panel(
                            "Console",
                            DockPanelPlacement{
                                .target = dock.stackFor(m_explorerPanel.item),
                                .zone = DockZone::bottom,
                            },
                            [this](UIComposer &panel) {
                                panel.label("Console", headingLabel());
                                panel.label("[info] UI demo mounted");
                                panel.label("[info] Drag a splitter to resize");
                                panel.label(
                                    "[info] Drag a tab out to float it");
                                panel.spacer();
                                panel.button("Clear", [this] {
                                    setStatus("Console cleared");
                                });
                            });

                        m_assetsPanel = dock.panel(
                            "Assets",
                            DockPanelPlacement{
                                .target = dock.stackFor(m_consolePanel.item),
                                .zone = DockZone::left,
                            },
                            [](UIComposer &panel) {
                                panel.label("Assets", headingLabel());
                                panel.label("Fonts");
                                panel.label("Icons");
                                panel.label("Textures");
                                panel.spacer();
                            });
                    });
                    m_dockSpace.updateLayout([](LayoutNode &layout) {
                        layout.setHeightAuto();
                        layout.setFlex(1.f, 1.f, 0.f);
                        layout.setMinSize({0.f, 180.f});
                    });

                    auto fotter = page.row({}, [this](UIComposer &footer) {
                        footer
                            .label("© 2024 Bess UI Core Demo",
                                   LabelOptions{.fontSize = 12.f})
                            .updateLayout([](LayoutNode &layout) {
                                layout.setMargin(
                                    Core::Style::Margin::fromSymmetric(4.f,
                                                                       2.f));
                            });
                        footer.spacer({.minimumSize = {0.f, 24}});
                    });

                    fotter.updateLayout([](LayoutNode &layout) {
                        layout.setHeightFitContent();
                    });
                });
            static_cast<void>(surface.layout(page, [](LayoutNode &layout) {
                layout.setWidthPercent(1.f);
                layout.setHeightPercent(1.f);
            }));
        });
        static_cast<void>(ui.layout(shell, stretch));
    }

    size_t UIDemoView::activationCount() const noexcept {
        return m_activationCount;
    }

    std::shared_ptr<TabModel> UIDemoView::tabs() const noexcept {
        return m_tabs;
    }

    std::shared_ptr<MenuModel> UIDemoView::menus() const noexcept {
        return m_menus;
    }

    WidgetRef<DockSpace> UIDemoView::dockSpace() const noexcept {
        return m_dockSpace;
    }

    void UIDemoView::setStatus(std::string text) {
        static_cast<void>(
            m_status.update(WidgetInvalidation::paint,
                            [text = std::move(text)](Label &status) mutable {
                                status.setText(std::move(text));
                            }));
    }

    void UIDemoView::incrementCounter() {
        ++m_activationCount;
        const auto label = std::format("Count: {}", m_activationCount);
        static_cast<void>(m_counterButton.update(
            WidgetInvalidation::paint,
            [&label](Button &button) { button.setLabel(label); }));
        setStatus(std::format("Button activated {} time{}",
                              m_activationCount,
                              m_activationCount == 1 ? "" : "s"));
    }

    void UIDemoView::selectNextTab() {
        if (m_tabs == nullptr || m_tabs->empty()) {
            return;
        }
        const TabId next = m_tabs->nextEnabled(m_tabs->active(), 1);
        if (next && m_tabs->activate(next)) {
            const auto *item = m_tabs->find(next);
            if (item != nullptr) {
                setStatus(std::format("Selected {} tab", item->title));
            }
        }
    }

} // namespace Bess::UI
