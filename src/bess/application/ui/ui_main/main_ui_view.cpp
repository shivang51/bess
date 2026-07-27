#include "ui/ui_main/main_ui_view.h"

#include "controls/basic_widgets.h"
#include "layout.h"
#include "ui_composer.h"
#include "widget_tree.h"

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

        [[nodiscard]] ActionDefinition
        shellAction(ActionId id,
                    std::string label,
                    std::vector<KeyChord> shortcuts,
                    ActionHandler invoked) {
            return {
                .id = std::move(id),
                .state = {.label = std::move(label)},
                .shortcuts = std::move(shortcuts),
                .invoked = std::move(invoked),
            };
        }
    } // namespace

    void MainUIView::registerShellActions(ActionRegistry &actions) {
        unregisterShellActions();

        const auto add = [this, &actions](ActionDefinition definition) {
            const ActionId id = definition.id;
            if (actions.registerAction(std::move(definition))) {
                m_shellActions.push_back(id);
            }
        };

        add(shellAction(
            ActionId{"shell.file.new"},
            "New",
            {{.key = KeyCode::n, .modifiers = KeyChordModifier::control}},
            [this](const ActionInvocation &) {
                setStatus("File > New (retained shell)");
            }));
        add(shellAction(
            ActionId{"shell.file.open"},
            "Open...",
            {{.key = KeyCode::o, .modifiers = KeyChordModifier::control}},
            [this](const ActionInvocation &) {
                setStatus("File > Open (retained shell)");
            }));
        add(shellAction(
            ActionId{"shell.file.save"},
            "Save",
            {{.key = KeyCode::s, .modifiers = KeyChordModifier::control}},
            [this](const ActionInvocation &) {
                setStatus("File > Save (retained shell)");
            }));
        add(shellAction(
            ActionId{"shell.file.quit"},
            "Quit",
            {{.key = KeyCode::f4, .modifiers = KeyChordModifier::alt}},
            [this](const ActionInvocation &) {
                setStatus("Quit is still handled by the app host");
            }));
        add(shellAction(ActionId{"shell.view.viewport"},
                        "Scene Viewport",
                        {},
                        [this](const ActionInvocation &) {
                            if (m_viewportPanel.show()) {
                                setStatus("Scene Viewport shown");
                            }
                        }));
        add(shellAction(ActionId{"shell.view.explorer"},
                        "Explorer",
                        {},
                        [this](const ActionInvocation &) {
                            if (m_explorerPanel.show()) {
                                setStatus("Explorer shown");
                            }
                        }));
        add(shellAction(ActionId{"shell.view.properties"},
                        "Properties",
                        {},
                        [this](const ActionInvocation &) {
                            if (m_propertiesPanel.show()) {
                                setStatus("Properties shown");
                            }
                        }));
        add(shellAction(ActionId{"shell.view.console"},
                        "Console",
                        {},
                        [this](const ActionInvocation &) {
                            if (m_consolePanel.show()) {
                                setStatus("Console shown");
                            }
                        }));
        add(shellAction(ActionId{"shell.help.about"},
                        "About",
                        {},
                        [this](const ActionInvocation &) {
                            setStatus(
                                "BESS retained UI shell + SceneView viewport");
                        }));
    }

    void MainUIView::unregisterShellActions() noexcept {
        if (m_actions != nullptr) {
            for (const auto &id : m_shellActions) {
                static_cast<void>(m_actions->unregisterAction(id));
            }
        }
        m_shellActions.clear();
    }

    void
    MainUIView::composeMenus(const std::shared_ptr<ActionRegistry> &actions) {
        m_actions = actions;
        registerShellActions(*actions);

        m_menus = std::make_shared<MenuModel>();
        static_cast<void>(m_menus->addMenu({
            .name = "File",
            .items =
                {
                    MenuItem::fromAction(ActionId{"shell.file.new"}),
                    MenuItem::fromAction(ActionId{"shell.file.open"}),
                    MenuItem::fromAction(ActionId{"shell.file.save"}),
                    MenuItem::separator(),
                    MenuItem::fromAction(ActionId{"shell.file.quit"}),
                },
        }));
        static_cast<void>(m_menus->addMenu({
            .name = "View",
            .items =
                {
                    MenuItem::fromAction(ActionId{"shell.view.viewport"}),
                    MenuItem::fromAction(ActionId{"shell.view.explorer"}),
                    MenuItem::fromAction(ActionId{"shell.view.properties"}),
                    MenuItem::fromAction(ActionId{"shell.view.console"}),
                },
        }));
        static_cast<void>(m_menus->addMenu({
            .name = "Help",
            .items =
                {
                    MenuItem::fromAction(ActionId{"shell.help.about"}),
                },
        }));
        m_menus->setActionRegistry(actions);
    }

    void MainUIView::compose(UIComposer &ui) {
        auto registry = ui.tree().actionRegistry();
        if (registry == nullptr) {
            registry = std::make_shared<ActionRegistry>();
            ui.tree().setActionRegistry(registry);
        }
        composeMenus(registry);

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
                        "Component Explorer",
                        [&] {
                            auto placement = sidePlacement(
                                dock.stackFor(m_viewportPanel.item),
                                DockZone::bottom);
                            // Catalog owns its own ScrollView for the tree.
                            // Zero padding so the card can fill the dock
                            // content slot; outer DockPanel then has nothing
                            // to scroll once nested overflow is opaque.
                            placement.content.padding = {};
                            placement.content.clipChildren = true;
                            return placement;
                        }(),
                        [this](UIComposer &panel) {
                            m_compCatalogView.compose(panel);
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
        unregisterShellActions();
        if (m_menus != nullptr) {
            m_menus->setActionRegistry(nullptr);
        }
        m_actions.reset();
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

    std::shared_ptr<SceneViewportController>
    MainUIView::primaryViewport() const noexcept {
        return m_sceneViewport != nullptr ? m_sceneViewport->primaryViewport()
                                          : nullptr;
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
