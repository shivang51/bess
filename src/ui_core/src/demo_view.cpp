#include "demo_view.h"

#include "bess_core/renderer/renderer_types.h"
#include "bess_core/style/bess_theme.h"

#include <format>
#include <utility>

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
                .stretchWidth = true,
                .stretchHeight = false,
                .clipChildren = false,
                .hitTestVisible = false,
            };
        }

        LabelOptions headingLabel() {
            return {
                .style =
                    UITextStyle{
                        .color =
                            Core::Renderer::Color::fromRGBA8(238, 240, 245),
                        .fontSize = 18.f,
                    },
            };
        }
    } // namespace

    void UIDemoView::compose(UIComposer &ui) {
        m_tabs = std::make_shared<TabModel>();
        static_cast<void>(m_tabs->add("Workspace", {}, false));
        static_cast<void>(m_tabs->add("Components", {}, false));
        static_cast<void>(m_tabs->add("Diagnostics", {}, false));

        auto shell = ui.surface([this](UIComposer &surface) {
            auto page =
                surface.column(columnOptions(), [this](UIComposer &page) {
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
                        const auto explorer =
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

                        const auto inspector = dock.panel(
                            "Inspector",
                            DockPanelPlacement{
                                .target = dock.stackFor(explorer.item),
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

                        static_cast<void>(dock.panel(
                            "Preview",
                            DockPanelPlacement{
                                .target = dock.stackFor(inspector.item),
                                .zone = DockZone::main,
                            },
                            [](UIComposer &panel) {
                                panel.label("Preview", headingLabel());
                                panel.label(
                                    "This shares a tab stack with Inspector.");
                                panel.spacer();
                                panel.button("Refresh preview");
                            }));

                        const auto console = dock.panel(
                            "Console",
                            DockPanelPlacement{
                                .target = dock.stackFor(explorer.item),
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

                        static_cast<void>(dock.panel(
                            "Assets",
                            DockPanelPlacement{
                                .target = dock.stackFor(console.item),
                                .zone = DockZone::left,
                            },
                            [](UIComposer &panel) {
                                panel.label("Assets", headingLabel());
                                panel.label("Fonts");
                                panel.label("Icons");
                                panel.label("Textures");
                                panel.spacer();
                            }));
                    });
                    m_dockSpace.updateLayout([](LayoutNode &layout) {
                        layout.setHeightAuto();
                        layout.setFlex(1.f, 1.f, 0.f);
                        layout.setMinSize({0.f, 180.f});
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
