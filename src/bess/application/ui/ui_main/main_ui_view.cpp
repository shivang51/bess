#include "ui/ui_main/main_ui_view.h"

#include "controls/basic_widgets.h"
#include "layout.h"

#include <functional>
#include <utility>

namespace Bess::UI {
    namespace {
        [[nodiscard]] LayoutSpec stretchFill() {
            return {.width = LayoutLength::percent(100.f),
                    .height = LayoutLength::percent(100.f)};
        }

        [[nodiscard]] FlexContainerOptions shellColumn() {
            return {
                .direction = LayoutDirection::vertical,
                .mainAxisAlignment = LayoutAlignment::start,
                .crossAxisAlignment = LayoutAlignment::start,
                .stretchWidth = true,
                .stretchHeight = true,
                .clipChildren = false,
                .hitTestVisible = false,
            };
        }

        [[nodiscard]] FlexContainerOptions menuHeaderOptions() {
            return {
                .direction = LayoutDirection::horizontal,
                .mainAxisAlignment = LayoutAlignment::start,
                .crossAxisAlignment = LayoutAlignment::center,
                .padding = Core::Style::Padding::fromHorizontal(6.f),
                .gap = 4.f,
                .stretchWidth = true,
                .stretchHeight = false,
                .clipChildren = false,
                .hitTestVisible = false,
            };
        }

        [[nodiscard]] FlexContainerOptions statusBarOptions() {
            return {
                .direction = LayoutDirection::horizontal,
                .mainAxisAlignment = LayoutAlignment::start,
                .crossAxisAlignment = LayoutAlignment::center,
                .padding = Core::Style::Padding::fromSymmetric(8.f, 4.f),
                .gap = 8.f,
                .stretchWidth = true,
                .stretchHeight = false,
                .clipChildren = false,
                .hitTestVisible = false,
            };
        }

        [[nodiscard]] DockPanelPlacement
        viewportPlacement(DockNodeId target = {},
                          DockZone zone = DockZone::main) {
            return {
                .target = target,
                .zone = zone,
                .closable = false,
                .content =
                    {
                        .direction = LayoutDirection::vertical,
                        .mainAxisAlignment = LayoutAlignment::start,
                        .crossAxisAlignment = LayoutAlignment::start,
                        .padding = {},
                        .stretchWidth = true,
                        .stretchHeight = true,
                        .clipChildren = true,
                        .hitTestVisible = false,
                    },
            };
        }

        [[nodiscard]] DockPanelPlacement sidePlacement(DockNodeId target,
                                                       DockZone zone) {
            return {
                .target = target,
                .zone = zone,
                .closable = true,
                .content =
                    {
                        .direction = LayoutDirection::vertical,
                        .mainAxisAlignment = LayoutAlignment::start,
                        .crossAxisAlignment = LayoutAlignment::start,
                        .padding = Core::Style::Padding{8.f},
                        .gap = 4.f,
                        .stretchWidth = true,
                        .stretchHeight = true,
                        .clipChildren = true,
                        .hitTestVisible = false,
                    },
            };
        }

        [[nodiscard]] MenuItem command(std::string name,
                                       std::string shortcut,
                                       std::function<void()> activated) {
            return {
                .name = std::move(name),
                .shortcut = std::move(shortcut),
                .activated = std::move(activated),
            };
        }
    } // namespace

    void MainUIView::composeMenus() {
        m_menus = std::make_shared<MenuModel>();
        static_cast<void>(m_menus->addMenu({
            .name = "File",
            .items =
                {
                    command(
                        "New",
                        "Ctrl+N",
                        [this] { setStatus("File > New (retained shell)"); }),
                    command(
                        "Open...",
                        "Ctrl+O",
                        [this] { setStatus("File > Open (retained shell)"); }),
                    command(
                        "Save",
                        "Ctrl+S",
                        [this] { setStatus("File > Save (retained shell)"); }),
                    MenuItem::separator(),
                    command("Quit",
                            "Alt+F4",
                            [this] {
                                setStatus(
                                    "Quit is still handled by the app host");
                            }),
                },
        }));
        static_cast<void>(m_menus->addMenu({
            .name = "View",
            .items =
                {
                    command("Scene Viewport",
                            "",
                            [this] {
                                if (m_viewportPanel.show()) {
                                    setStatus("Scene Viewport shown");
                                }
                            }),
                    command("Explorer",
                            "",
                            [this] {
                                if (m_explorerPanel.show()) {
                                    setStatus("Explorer shown");
                                }
                            }),
                    command("Properties",
                            "",
                            [this] {
                                if (m_propertiesPanel.show()) {
                                    setStatus("Properties shown");
                                }
                            }),
                    command("Console",
                            "",
                            [this] {
                                if (m_consolePanel.show()) {
                                    setStatus("Console shown");
                                }
                            }),
                },
        }));
        static_cast<void>(m_menus->addMenu({
            .name = "Help",
            .items =
                {
                    command(
                        "About",
                        "",
                        [this] {
                            setStatus(
                                "BESS retained UI shell + SceneView viewport");
                        }),
                },
        }));
    }

    void MainUIView::compose(UIComposer &ui) {
        composeMenus();
        m_sceneViewport = std::make_unique<Canvas::SceneViewport>();

        auto shell = ui.surface([this](UIComposer &surface) {
            auto page = surface.column(shellColumn(), [this](UIComposer &page) {
                auto header =
                    page.row(menuHeaderOptions(), [this](UIComposer &row) {
                        row.label("BESS", LabelOptions{.fontSize = 13.f});
                        row.gap(6.f);
                        row.menuBar(m_menus);
                        row.spacer(SpacerOptions{.minimumSize = {0.f, 26.f}});
                    });
                header.setLayout({.flexShrink = 0.f});

                m_dockSpace = page.dockSpace([this](DockComposer &dock) {
                    m_viewportPanel =
                        dock.panel("Scene Viewport",
                                   viewportPlacement(),
                                   [this](UIComposer &panel) {
                                       m_sceneViewport->compose(panel);
                                   });

                    m_explorerPanel = dock.panel(
                        "Explorer",
                        sidePlacement(dock.stackFor(m_viewportPanel.item),
                                      DockZone::left),
                        [](UIComposer &panel) {
                            panel.label("Project Explorer");
                            panel.label("Migrating from ImGui — placeholder");
                            panel.spacer();
                        });

                    m_propertiesPanel = dock.panel(
                        "Properties",
                        sidePlacement(dock.stackFor(m_viewportPanel.item),
                                      DockZone::right),
                        [](UIComposer &panel) {
                            panel.label("Properties");
                            panel.label("Migrating from ImGui — placeholder");
                            panel.spacer();
                        });

                    m_consolePanel = dock.panel(
                        "Console",
                        sidePlacement(dock.stackFor(m_viewportPanel.item),
                                      DockZone::bottom),
                        [](UIComposer &panel) {
                            panel.label("Console");
                            panel.label("Migrating from ImGui — placeholder");
                            panel.spacer();
                        });
                });
                m_dockSpace.setLayout({
                    .height = LayoutLength::autoSize(),
                    .minSize = glm::vec2{0.f, 200.f},
                    .flexGrow = 1.f,
                    .flexShrink = 1.f,
                    .flexBasis = 0.f,
                });

                auto status =
                    page.row(statusBarOptions(), [this](UIComposer &row) {
                        row.label("Ready");
                        row.spacer();
                        m_status = row.label(
                            "Retained SceneView viewport active",
                            LabelOptions{
                                .fontSize = 12.f,
                                .horizontal = HorizontalTextAlignment::end,
                                .autoSize = false,
                            });
                        m_status.setLayout({
                            .width = LayoutLength::autoSize(),
                            .height = 18.f,
                            .minSize = glm::vec2{120.f, 18.f},
                            .flexGrow = 1.f,
                            .flexShrink = 1.f,
                            .flexBasis = 0.f,
                        });
                    });
                status.setLayout({.flexShrink = 0.f});
            });
            static_cast<void>(surface.layout(page, stretchFill()));
        });
        static_cast<void>(shell.setLayout(stretchFill()));
    }

    void MainUIView::onUnmounting(UIViewContext &context) noexcept {
        m_sceneViewport.reset();
        m_dockSpace = {};
        m_status = {};
        m_viewportPanel = {};
        m_explorerPanel = {};
        m_propertiesPanel = {};
        m_consolePanel = {};
        m_menus.reset();
        UIView::onUnmounting(context);
    }

    WidgetRef<DockSpace> MainUIView::dockSpace() const noexcept {
        return m_dockSpace;
    }

    void MainUIView::setStatus(std::string text) {
        if (!m_status) {
            return;
        }
        m_status.mutate([text = std::move(text)](Label &label) mutable {
            label.setText(std::move(text));
        });
    }

} // namespace Bess::UI
