#include "demo_view.h"
#include "bess_core/style/bess_theme.h"
#include "controls/drag_drop_widgets.h"
#include "controls/reorderable_list.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <format>
#include <functional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace Bess::UI {
    struct UIDemoReorderState {
        struct Item {
            ReorderItemId id;
            std::string label;
            WidgetRef<DraggableListItem> widget;
        };

        ReorderListId list;
        WidgetRef<ReorderableList> widget;
        std::vector<Item> items;
    };

    namespace {
        LayoutSpec stretch() {
            return {.width = LayoutLength::stretch(),
                    .height = LayoutLength::stretch()};
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

        [[nodiscard]] DragPayload demoTextPayload() {
            DragPayloadBuilder builder;
            static_cast<void>(builder.set(DragFormats::plainText,
                                          std::string{"Bess demo payload"}));
            return std::move(builder).build();
        }

        [[nodiscard]] DragOperation
        supportedDropOperation(const DragTargetEvent &event) noexcept {
            if (event.payload.get(DragFormats::plainText) == nullptr &&
                event.payload.get(DragFormats::files) == nullptr) {
                return DragOperation::none;
            }
            if (isSingleDragOperation(event.requestedOperation) &&
                hasDragOperation(event.allowedOperations,
                                 event.requestedOperation)) {
                return event.requestedOperation;
            }
            if (hasDragOperation(event.allowedOperations,
                                 DragOperation::copy)) {
                return DragOperation::copy;
            }
            if (hasDragOperation(event.allowedOperations,
                                 DragOperation::move)) {
                return DragOperation::move;
            }
            return DragOperation::none;
        }

        [[nodiscard]] bool containsItem(std::span<const ReorderItemId> items,
                                        ReorderItemId candidate) {
            return std::ranges::find(items, candidate) != items.end();
        }

        [[nodiscard]] bool
        reorderDemoModel(const std::shared_ptr<UIDemoReorderState> &state,
                         const ReorderRequest &request) {
            if (state == nullptr || !state->widget ||
                request.operation != DragOperation::move ||
                request.source != state->list ||
                request.target != state->list || request.items.empty()) {
                return false;
            }

            std::vector<UIDemoReorderState::Item> moving;
            std::vector<UIDemoReorderState::Item> remaining;
            moving.reserve(request.items.size());
            remaining.reserve(state->items.size());
            for (const auto &item : state->items) {
                (containsItem(request.items, item.id) ? moving : remaining)
                    .push_back(item);
            }
            if (moving.size() != request.items.size()) {
                return false;
            }

            auto insertion = remaining.end();
            if (request.before) {
                insertion = std::ranges::find(
                    remaining, request.before, &UIDemoReorderState::Item::id);
                if (insertion == remaining.end()) {
                    return false;
                }
            }
            remaining.insert(insertion, moving.begin(), moving.end());

            auto *tree = state->widget.tree();
            if (tree == nullptr) {
                return false;
            }
            for (const auto &item : remaining) {
                if (!item.widget ||
                    tree->getParent(item.widget.id()) != state->widget.id()) {
                    return false;
                }
            }
            for (size_t index = 0; index < remaining.size(); ++index) {
                if (!tree->reparentWidget(remaining[index].widget.id(),
                                          state->widget.id(),
                                          index)) {
                    return false;
                }
            }
            state->items = std::move(remaining);
            return true;
        }

        void composeDemoCard(UIComposer &owner,
                             std::string label,
                             UIBoxStyle style) {
            auto card = owner.surface(
                SurfaceOptions{.style = std::move(style)},
                [label = std::move(label)](UIComposer &surface) mutable {
                    surface.row(
                        FlexContainerOptions{
                            .direction = LayoutDirection::horizontal,
                            .mainAxisAlignment = LayoutAlignment::start,
                            .crossAxisAlignment = LayoutAlignment::center,
                            .padding =
                                Core::Style::Padding::fromHorizontal(8.f),
                            .stretchWidth = true,
                            .stretchHeight = true,
                            .clipChildren = true,
                            .hitTestVisible = false,
                        },
                        [&label](UIComposer &row) { row.label(label); });
                });
            card.setLayout({
                .width = LayoutLength::percent(100.f),
                .height = LayoutLength::percent(100.f),
            });
        }

        class DemoRenderDelegate final : public IRenderViewDelegate {
          public:
            explicit DemoRenderDelegate(const UITheme &theme)
                : m_background(theme.panel.background),
                  m_primary(theme.slider.fill.background),
                  m_secondary(theme.button.hovered.background),
                  m_detail(theme.label.color) {
            }

            void render(RenderViewFrameContext &context) override {
                using namespace Core::Renderer;
                const glm::vec2 extent{
                    static_cast<float>(context.extent.width),
                    static_cast<float>(context.extent.height),
                };
                if (extent.x <= 0.f || extent.y <= 0.f) {
                    return;
                }

                context.renderer.drawQuad({
                    .position = {0.f, 0.f},
                    .size = extent,
                    .color = m_background,
                    .transformMode = RenderTransformMode::Screen,
                });
                context.renderer.drawQuad({
                    .position = {-extent.x * 0.16f, 0.f},
                    .size = {extent.x * 0.52f, extent.y * 0.5f},
                    .rotation = -0.08f,
                    .color = m_primary,
                    .transformMode = RenderTransformMode::Screen,
                    .radius = glm::vec4{10.f},
                });
                context.renderer.drawCircle({
                    .position = {extent.x * 0.24f, 0.f},
                    .radius = std::min(extent.x, extent.y) * 0.19f,
                    .color = m_secondary,
                    .transformMode = RenderTransformMode::Screen,
                });
                context.renderer.drawLine({
                    .p0 = {-extent.x * 0.4f, extent.y * 0.34f},
                    .p1 = {extent.x * 0.4f, extent.y * 0.34f},
                    .thickness = 1.f,
                    .color = m_detail,
                    .transformMode = RenderTransformMode::Screen,
                });
            }

          private:
            Core::Renderer::Color m_background;
            Core::Renderer::Color m_primary;
            Core::Renderer::Color m_secondary;
            Core::Renderer::Color m_detail;
        };
    } // namespace

    UIDemoView::~UIDemoView() {
        unregisterShowcaseAction();
    }

    void UIDemoView::compose(UIComposer &ui) {
        unregisterShowcaseAction();
        m_actionRegistry = ui.tree().actionRegistry();
        m_showcaseAction =
            ActionId{std::format("ui.demo.showcase.{}",
                                 static_cast<unsigned long long>(
                                     reinterpret_cast<std::uintptr_t>(this)))};
        if (m_actionRegistry != nullptr) {
            const auto registered = m_actionRegistry->registerAction({
                .id = m_showcaseAction,
                .state = {.label = "Run registered action",
                          .description =
                              "Exercises shared action state and dispatch"},
                .shortcuts = {{.key = KeyCode::f12,
                               .modifiers = KeyChordModifier::control |
                                            KeyChordModifier::shift}},
                .invoked =
                    [this](const ActionInvocation &) {
                        incrementCounter();
                        setStatus("Registered action invoked (Ctrl+Shift+F12)");
                    },
            });
            m_showcaseActionRegistered = static_cast<bool>(registered);
        }

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
                                     }),
                             command("",
                                     "Controls",
                                     "",
                                     [this] {
                                         if (m_controlsPanel.show()) {
                                             setStatus("Controls shown");
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
            auto page = surface.column(columnOptions(), [this](UIComposer &page) {
                auto menuHeader =
                    page.row(menuHeaderOptions(), [this](UIComposer &header) {
                        header.label("B", applicationIconLabel());
                        header.gap(4.f);
                        m_menuBar = header.menuBar(m_menus);
                        header.spacer(
                            SpacerOptions{.minimumSize = {0.f, 26.f}});
                        auto action = header.button(
                            "?", [this] { setStatus("Toolbar action"); });
                        action.setLayout({.width = 20.f,
                                          .height = 20.f,
                                          .minSize = glm::vec2{20.f, 20.f}});
                    });
                menuHeader.setLayout({.flexShrink = 0.f});
                auto header =
                    page.row(headerOptions(), [this](UIComposer &header) {
                        header.label("Bess UI Core", headingLabel());
                        m_status = header.label(
                            "Drag a dock tab to float it; use the node "
                            "guides to dock it again",
                            LabelOptions{.horizontal =
                                             HorizontalTextAlignment::end,
                                         .autoSize = false});
                        m_status.setLayout({
                            .width = LayoutLength::autoSize(),
                            .height = 24.f,
                            .minSize = glm::vec2{80.f, 24.f},
                            .flexGrow = 1.f,
                            .flexShrink = 1.f,
                            .flexBasis = 0.f,
                        });

                        header.button("Add tab", [this] {
                            m_tabs->add(
                                std::format("New tab {}", m_tabs->size()));
                        });

                        header.button("Next tab", [this] { selectNextTab(); });
                        m_counterButton = header.button(
                            "Count: 0",
                            [this] { incrementCounter(); },
                            ButtonOptions{.autoSize = false});
                        m_counterButton.setLayout(
                            {.width = 78.f, .height = 26.f});

                        auto disabled = header.button("Disabled");
                        static_cast<void>(disabled.setEnabled(false));
                    });
                header.setLayout({.flexShrink = 0.f});

                auto tabLayer =
                    page.stack(StackContainerOptions{.stretchHeight = false},
                               [this](UIComposer &tabs) {
                                   m_tabBar = tabs.tabBar(m_tabs);
                               });
                tabLayer.setLayout({.flexShrink = 0.f});

                m_dockSpace = page.dockSpace([this](DockComposer &dock) {
                    m_explorerPanel =
                        dock.panel("Explorer", [this](UIComposer &panel) {
                            panel.label("Project Explorer", headingLabel());
                            panel.label("assets/");
                            panel.label("plugins/");
                            panel.label("src/");
                            panel.spacer();
                            panel.button("Create item", [this] {
                                setStatus("Explorer: create item activated");
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

                    m_controlsPanel = dock.panel(
                        "Controls",
                        DockPanelPlacement{
                            .target = dock.stackFor(m_inspectorPanel.item),
                            .zone = DockZone::main,
                        },
                        [this](UIComposer &panel) {
                            panel.scrollView(
                                {.horizontal = false,
                                 .vertical = true,
                                 .clipContent = true},
                                [this](UIComposer &viewport) {
                                    FlexContainerOptions controlsOptions{
                                        .direction = LayoutDirection::vertical,
                                        .mainAxisAlignment =
                                            LayoutAlignment::start,
                                        .crossAxisAlignment =
                                            LayoutAlignment::start,
                                        .padding = Core::Style::Padding{8.f},
                                        .gap = 8.f,
                                        .stretchWidth = true,
                                        .stretchHeight = false,
                                        .clipChildren = false,
                                        .hitTestVisible = false,
                                    };
                                    viewport.column(
                                        controlsOptions,
                                        [this](UIComposer &controls) {
                                            controls.label("Input controls",
                                                           headingLabel());

                                            auto text = controls.textBox(
                                                std::make_shared<TextEditModel>(
                                                    "Editable text"),
                                                [this](const auto &value) {
                                                    setStatus("Text: " + value);
                                                },
                                                [this](const auto &value) {
                                                    setStatus("Submitted: " +
                                                              value);
                                                },
                                                {.placeholder = "Type here"});
                                            text.setLayout({
                                                .width = LayoutLength::percent(
                                                    100.f),
                                            });

                                            auto autocomplete = controls.autocomplete(
                                                std::make_shared<
                                                    TextEditModel>(),
                                                [](std::string_view query) {
                                                    const std::vector<
                                                        std::string>
                                                        values{
                                                            "Explorer",
                                                            "Inspector",
                                                            "Console",
                                                            "Components",
                                                            "Diagnostics",
                                                        };
                                                    std::string needle{query};
                                                    std::ranges::transform(
                                                        needle,
                                                        needle.begin(),
                                                        [](unsigned char
                                                               value) {
                                                            return static_cast<
                                                                char>(
                                                                std::tolower(
                                                                    value));
                                                        });
                                                    std::vector<
                                                        AutocompleteItem>
                                                        result;
                                                    for (const auto &value :
                                                         values) {
                                                        std::string candidate =
                                                            value;
                                                        std::ranges::transform(
                                                            candidate,
                                                            candidate.begin(),
                                                            [](unsigned char
                                                                   ch) {
                                                                return static_cast<
                                                                    char>(
                                                                    std::tolower(
                                                                        ch));
                                                            });
                                                        if (candidate.contains(
                                                                needle)) {
                                                            result.push_back(
                                                                {.label = value,
                                                                 .replacement =
                                                                     value,
                                                                 .detail =
                                                                     "panel"});
                                                        }
                                                    }
                                                    return result;
                                                },
                                                {},
                                                {},
                                                [this](const auto &item) {
                                                    setStatus("Completed: " +
                                                              item.label);
                                                },
                                                {.textBox = {
                                                     .placeholder =
                                                         "Find a panel"}});
                                            autocomplete.setLayout({
                                                .width = LayoutLength::percent(
                                                    100.f),
                                            });

                                            const auto checkModel =
                                                std::make_shared<
                                                    CheckStateModel>();
                                            controls.checkBox(
                                                "Tri-state checkbox",
                                                checkModel,
                                                [this](CheckState) {
                                                    setStatus(
                                                        "Checkbox changed");
                                                },
                                                {.cycleMixed = true});
                                            controls.toggle(
                                                "Toggle switch",
                                                {},
                                                [this](bool enabled) {
                                                    setStatus(
                                                        enabled ? "Toggle on"
                                                                : "Toggle off");
                                                });

                                            const auto radioGroup =
                                                std::make_shared<
                                                    RadioGroupModel>();
                                            controls.focusScope(
                                                FocusScopeOptions{
                                                    .focus =
                                                        {.trapFocus = false,
                                                         .autoFocus = false,
                                                         .restoreFocus = true},
                                                    .container =
                                                        {.direction =
                                                             LayoutDirection::
                                                                 horizontal,
                                                         .mainAxisAlignment =
                                                             LayoutAlignment::
                                                                 start,
                                                         .crossAxisAlignment =
                                                             LayoutAlignment::
                                                                 center,
                                                         .gap = 12.f,
                                                         .stretchWidth = true,
                                                         .stretchHeight =
                                                             false}},
                                                [radioGroup](
                                                    UIComposer &radios) {
                                                    radios.radio("Alpha",
                                                                 radioGroup);
                                                    radios.radio("Beta",
                                                                 radioGroup);
                                                });

                                            auto sliderModel =
                                                std::make_shared<RangeModel>(
                                                    0.0, 100.0, 35.0, 1.0);
                                            auto slider = controls.slider(
                                                sliderModel,
                                                [this](double value) {
                                                    setStatus(std::format(
                                                        "Slider: {:.0f}",
                                                        value));
                                                });
                                            slider.setLayout({
                                                .width = LayoutLength::percent(
                                                    100.f),
                                            });

                                            auto dropdownModel =
                                                std::make_shared<
                                                    DropdownModel>();
                                            static_cast<void>(
                                                dropdownModel->add("Compact"));
                                            static_cast<void>(
                                                dropdownModel->add(
                                                    "Comfortable"));
                                            static_cast<void>(
                                                dropdownModel->add("Spacious"));
                                            controls.dropdown(
                                                dropdownModel,
                                                [this](DropdownItemId) {
                                                    setStatus(
                                                        "Dropdown changed");
                                                });

                                            controls.tooltip(
                                                "Tooltips use PopupHost",
                                                [this](UIComposer &tip) {
                                                    tip.button(
                                                        "Hover for tooltip",
                                                        [this] {
                                                            setStatus("Tooltip "
                                                                      "button");
                                                        });
                                                });

                                            auto contextModel =
                                                std::make_shared<MenuModel>();
                                            const MenuId contextMenu =
                                                contextModel->addMenu(
                                                    {.name = "Context",
                                                     .items = {
                                                         command(
                                                             "",
                                                             "Rename",
                                                             "F2",
                                                             [this] {
                                                                 setStatus(
                                                                     "Rename "
                                                                     "from "
                                                                     "context "
                                                                     "menu");
                                                             }),
                                                         command(
                                                             "",
                                                             "More",
                                                             "",
                                                             {},
                                                             {command(
                                                                 "",
                                                                 "Details",
                                                                 "",
                                                                 [this] {
                                                                     setStatus(
                                                                         "Conte"
                                                                         "xt "
                                                                         "subme"
                                                                         "nu");
                                                                 })})}});
                                            controls.contextMenu(
                                                contextModel,
                                                contextMenu,
                                                [this](UIComposer &region) {
                                                    region.button(
                                                        "Right-click me",
                                                        [this] {
                                                            setStatus(
                                                                "Context "
                                                                "region "
                                                                "clicked");
                                                        });
                                                });

                                            composeGenericShowcase(controls);
                                        });
                                });
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
                            panel.label("[info] Drag a tab out to float it");
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
                m_dockSpace.setLayout({
                    .height = LayoutLength::autoSize(),
                    .minSize = glm::vec2{0.f, 180.f},
                    .flexGrow = 1.f,
                    .flexShrink = 1.f,
                    .flexBasis = 0.f,
                });

                auto footerRow = page.row({}, [this](UIComposer &footer) {
                    footer
                        .label("© 2024 Bess UI Core Demo",
                               LabelOptions{.fontSize = 12.f})
                        .setLayout({
                            .margin =
                                Core::Style::Margin::fromSymmetric(4.f, 2.f),
                        });
                    footer.spacer({.minimumSize = {0.f, 24}});
                });

                footerRow.setLayout({.height = LayoutLength::fitContent()});
            });
            static_cast<void>(
                surface.layout(page,
                               {.width = LayoutLength::percent(100.f),
                                .height = LayoutLength::percent(100.f)}));
        });
        static_cast<void>(shell.setLayout(stretch()));
    }

    void UIDemoView::composeGenericShowcase(UIComposer &controls) {
        controls.label("Generic retained controls", headingLabel());

        FlexContainerOptions horizontalGroup{
            .direction = LayoutDirection::horizontal,
            .mainAxisAlignment = LayoutAlignment::start,
            .crossAxisAlignment = LayoutAlignment::center,
            .gap = 8.f,
            .stretchWidth = true,
            .stretchHeight = false,
            .clipChildren = false,
            .hitTestVisible = false,
        };
        controls.row(horizontalGroup, [this](UIComposer &actions) {
            actions.actionButton(m_showcaseAction);
            actions.label("Ctrl+Shift+F12", LabelOptions{.fontSize = 12.f});
        });

        controls.treeNode(
            "Retained tree node",
            TreeNodeOptions{.expanded = true, .contentGap = 3.f},
            [this](bool expanded) {
                setStatus(expanded ? "Tree node expanded"
                                   : "Tree node collapsed");
            },
            [this](UIComposer &branch) {
                branch.label("Leaf content remains ordinary widgets");
                branch.treeNode(
                    "Nested branch",
                    TreeNodeOptions{.expanded = false},
                    [this](bool expanded) {
                        setStatus(expanded ? "Nested branch expanded"
                                           : "Nested branch collapsed");
                    },
                    [](UIComposer &nested) { nested.label("Nested leaf"); });
            });

        const auto &theme = controls.tree().theme();
        controls.label("Texture image placeholder");
        auto image = controls.image(std::shared_ptr<Core::Renderer::ITexture>{},
                                    ImageOptions{
                                        .fit = ImageFit::contain,
                                        .cornerRadius = glm::vec4{5.f},
                                        .autoSize = false,
                                        .fallbackSize = {180.f, 54.f},
                                        .placeholder = theme.button.normal,
                                    });
        image.setLayout({
            .width = 180.f,
            .height = 54.f,
            .minSize = glm::vec2{80.f, 40.f},
        });

        controls.label("Renderer-independent offscreen view");
        RenderViewOptions renderOptions;
        renderOptions.policy = RenderPolicy::onDemand;
        renderOptions.frame.clearColor = theme.panel.background;
        renderOptions.cornerRadius = glm::vec4{6.f};
        renderOptions.focusable = false;
        renderOptions.hitTestVisible = false;
        auto renderView = controls.renderView(
            std::make_shared<DemoRenderDelegate>(theme), renderOptions);
        renderView.setLayout({
            .width = LayoutLength::percent(100.f),
            .height = 92.f,
            .minSize = glm::vec2{160.f, 64.f},
        });

        controls.label("Typed drag and drop");
        controls.row(horizontalGroup, [this](UIComposer &dragRow) {
            const auto &dragTheme = dragRow.tree().theme();

            DraggableOptions source;
            source.payload = demoTextPayload();
            source.allowedOperations = DragOperation::copy;
            source.preferredOperation = DragOperation::copy;
            source.callbacks.onStarted = [this](const auto &) {
                setStatus("Dragging typed text payload");
            };
            source.callbacks.onCompleted = [this](const auto &completed) {
                if (!completed.accepted) {
                    setStatus("Drag canceled or rejected");
                }
            };
            auto draggable = dragRow.emplace<Draggable>(
                dragRow.tree().dragDrop(), std::move(source));
            draggable.setLayout({
                .width = 150.f,
                .height = 30.f,
                .minSize = glm::vec2{110.f, 30.f},
            });
            UIComposer dragContent{dragRow.tree(), draggable.id()};
            composeDemoCard(
                dragContent, "Drag payload", dragTheme.button.normal);

            DropZoneOptions target;
            target.callbacks.propose = [](const auto &event) {
                return DragProposal{supportedDropOperation(event)};
            };
            target.callbacks.onDrop = [this](const auto &event) {
                if (const auto *files = event.payload.get(DragFormats::files);
                    files != nullptr) {
                    setStatus(std::format("Dropped {} file{}",
                                          files->paths.size(),
                                          files->paths.size() == 1 ? "" : "s"));
                    return true;
                }
                if (const auto *text =
                        event.payload.get(DragFormats::plainText);
                    text != nullptr) {
                    setStatus("Dropped: " + *text);
                    return true;
                }
                return false;
            };
            target.dragOverStyle = dragTheme.dock.dropPreview;
            auto dropZone = dragRow.emplace<DropZone>(dragRow.tree().dragDrop(),
                                                      std::move(target));
            dropZone.setLayout({
                .width = 190.f,
                .height = 30.f,
                .minSize = glm::vec2{130.f, 30.f},
                .flexShrink = 1.f,
            });
            UIComposer dropContent{dragRow.tree(), dropZone.id()};
            composeDemoCard(
                dropContent, "Drop text/files here", dragTheme.panel);
        });

        controls.label("Model-driven reorderable list");
        m_reorderState = std::make_shared<UIDemoReorderState>();
        m_reorderState->list = ReorderListId::generate();

        ReorderableListOptions listOptions;
        listOptions.gap = 4.f;
        listOptions.allowCrossList = false;
        listOptions.onReorder = [this, state = std::weak_ptr{m_reorderState}](
                                    const ReorderRequest &request) {
            const bool reordered = reorderDemoModel(state.lock(), request);
            if (reordered) {
                setStatus("Reordered list model");
            }
            return reordered;
        };
        m_reorderState->widget =
            controls.emplace<ReorderableList>(controls.tree().dragDrop(),
                                              std::move(listOptions),
                                              m_reorderState->list);
        m_reorderState->widget.setLayout({
            .width = LayoutLength::percent(100.f),
            .height = 92.f,
            .minSize = glm::vec2{140.f, 92.f},
        });

        UIComposer list{controls.tree(), m_reorderState->widget.id()};
        for (std::string label : {
                 "First model item",
                 "Second model item",
                 "Third model item",
             }) {
            UIDemoReorderState::Item item{
                .id = ReorderItemId::generate(),
                .label = std::move(label),
            };
            item.widget = list.emplace<DraggableListItem>(
                list.tree().dragDrop(), m_reorderState->list, item.id);
            item.widget.setLayout({
                .width = LayoutLength::percent(100.f),
                .height = 28.f,
                .minSize = glm::vec2{120.f, 28.f},
            });
            UIComposer itemContent{list.tree(), item.widget.id()};
            composeDemoCard(itemContent, item.label, theme.tabs.normal);
            m_reorderState->items.push_back(std::move(item));
        }
    }

    void UIDemoView::onUnmounting(UIViewContext &context) noexcept {
        unregisterShowcaseAction();
        UIView::onUnmounting(context);
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

    void UIDemoView::unregisterShowcaseAction() noexcept {
        if (m_actionRegistry != nullptr && m_showcaseActionRegistered &&
            m_showcaseAction) {
            try {
                static_cast<void>(
                    m_actionRegistry->unregisterAction(m_showcaseAction));
            } catch (...) {
                // UIView teardown is noexcept and may run during host teardown.
            }
        }
        m_showcaseActionRegistered = false;
        m_showcaseAction = {};
        m_actionRegistry.reset();
    }

} // namespace Bess::UI
