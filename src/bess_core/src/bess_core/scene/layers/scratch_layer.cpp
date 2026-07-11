#include "bess_core/scene/layers/scratch_layer.h"
#include "bess_core/scene/scene_draw_context.h"
#include "bess_core/scene/scene_ui/controls/button_comp.h"
#include "bess_core/scene/scene_ui/controls/checkbox_comp.h"
#include "bess_core/scene/scene_ui/controls/container_comp.h"
#include "bess_core/scene/scene_ui/controls/context_menu_comp.h"
#include "bess_core/scene/scene_ui/controls/dropdown_comp.h"
#include "bess_core/scene/scene_ui/controls/image_comp.h"
#include "bess_core/scene/scene_ui/controls/label_comp.h"
#include "bess_core/scene/scene_ui/controls/list_box_comp.h"
#include "bess_core/scene/scene_ui/controls/progress_bar_comp.h"
#include "bess_core/scene/scene_ui/controls/scalar_input_comp.h"
#include "bess_core/scene/scene_ui/controls/selectable_button_comp.h"
#include "bess_core/scene/scene_ui/controls/segmented_button_comp.h"
#include "bess_core/scene/scene_ui/controls/slider_comp.h"
#include "bess_core/scene/scene_ui/controls/tree_node_comp.h"
#include <string>

namespace Bess::Canvas {
    namespace {
        constexpr float kInitialDemoValue = 35.f;

        std::shared_ptr<UI::ContainerComp> demoPanel = nullptr;
        std::shared_ptr<UI::SliderComp> demoSlider = nullptr;
        std::shared_ptr<UI::ProgressBarComp> demoProgress = nullptr;
        std::shared_ptr<UI::ScalarInputComp> demoScalar = nullptr;
        std::shared_ptr<UI::ImageComp> demoImage = nullptr;
        std::shared_ptr<UI::ListBoxComp> demoList = nullptr;
    } // namespace

    void ScratchLayer::init(SceneLifecycleContext &ctx) {
        auto title = UI::LabelComp::create(
            "Retained UI Controls",
            {
                .sceneState = ctx.sceneState,
                .style = UI::UIElementStyle{
                    .margin = Core::Style::Margin::onlyBottom(4.f),
                    .fontSize = 10.f,
                },
            });

        auto dropdown = UI::DropdownComp::create(
            {
                {.label = "Balanced"},
                {.label = "Performance"},
                {.label = "Quality"},
                {.label = "Disabled option", .enabled = false},
                {.label = "Experimental"},
            },
            0,
            [title](size_t, const UI::UIDropdownOption &option) {
                title->setName("Mode: " + option.label);
            },
            {
                .sceneState = ctx.sceneState,
            });
        dropdown->setHeaderSize({150.f, 22.f});

        auto menu = UI::ContextMenuComp::create(
            {
                {.label = "Reset output",
                 .callback =
                     []() {
                         if (demoSlider) {
                             demoSlider->setValue(kInitialDemoValue);
                         }
                         if (demoProgress) {
                             demoProgress->setValue(kInitialDemoValue);
                         }
                         if (demoScalar) {
                             demoScalar->setValue(kInitialDemoValue);
                         }
                     }},
                {.separator = true},
                {.label = "Mark title",
                 .callback = [title]() { title->setName("Context action"); }},
                {.label = "Disabled action", .enabled = false},
            },
            "Right click actions",
            {
                .sceneState = ctx.sceneState,
            });
        menu->setTriggerSize({150.f, 22.f});

        auto selectable = UI::SelectableButtonComp::create(
            "Selected",
            [](bool selected) {
                if (demoImage) {
                    demoImage->setTintColor(
                        selected
                            ? Core::Renderer::Color{1.f, 1.f, 1.f, 1.f}
                            : Core::Renderer::Color{0.72f, 0.72f, 0.72f, 1.f});
                }
            },
            true,
            {
                .sceneState = ctx.sceneState,
            });
        selectable->setButtonSize({150.f, 22.f});

        auto imageMode = UI::SegmentedButtonComp::create(
            {
                {.label = "Fit"},
                {.label = "Fill"},
                {.label = "Stretch"},
            },
            0,
            [title](size_t index, const UI::UISegmentedButtonOption &option) {
                if (demoImage) {
                    switch (index) {
                    case 1:
                        demoImage->setFit(UI::UIImageFit::Cover);
                        break;
                    case 2:
                        demoImage->setFit(UI::UIImageFit::Stretch);
                        break;
                    case 0:
                    default:
                        demoImage->setFit(UI::UIImageFit::Contain);
                        break;
                    }
                }
                title->setName("Image mode: " + option.label);
            },
            {
                .sceneState = ctx.sceneState,
            });
        imageMode->setSegmentSize({50.f, 22.f});

        demoImage = UI::ImageComp::create(
            {150.f, 46.f},
            {
                .sceneState = ctx.sceneState,
            });
        demoImage->setSourceFile("assets/images/logo/BessLogo.png");
        demoImage->setFit(UI::UIImageFit::Contain);
        demoImage->setDrawBackground(true);
        demoImage->setCornerRadius(glm::vec4(6.f));

        auto listInput = UI::SelectableButtonComp::create(
            "Inputs",
            [title](bool selected) {
                title->setName(selected ? "List widget: Inputs"
                                        : "List widget");
            },
            false,
            {
                .sceneState = ctx.sceneState,
                .style = UI::UIElementStyle{
                    .margin = Core::Style::Margin::onlyBottom(2.f),
                },
            });
        listInput->setButtonSize({124.f, 20.f});

        auto listOutput = UI::SelectableButtonComp::create(
            "Outputs",
            [title](bool selected) {
                title->setName(selected ? "List widget: Outputs"
                                        : "List widget");
            },
            true,
            {
                .sceneState = ctx.sceneState,
                .style = UI::UIElementStyle{
                    .margin = Core::Style::Margin::onlyBottom(2.f),
                },
            });
        listOutput->setButtonSize({124.f, 20.f});

        auto listDiagnostics = UI::CheckboxComp::create(
            "Diagnostics",
            [title](bool checked) {
                title->setName(checked ? "List widget: Diagnostics on"
                                       : "List widget: Diagnostics off");
            },
            true,
            {
                .sceneState = ctx.sceneState,
                .style = UI::UIElementStyle{
                    .margin = Core::Style::Margin::onlyBottom(2.f),
                },
            });

        auto listScalar = UI::ScalarInputComp::create(
            12.0,
            [title](double value) {
                title->setName("List scalar: " + std::to_string(
                                                    static_cast<int>(value)));
            },
            {
                .sceneState = ctx.sceneState,
                .style = UI::UIElementStyle{
                    .margin = Core::Style::Margin::onlyBottom(2.f),
                },
            });
        listScalar->setInputSize({124.f, 20.f});
        listScalar->setValueRange(0.0, 64.0);
        listScalar->setStep(1.0);
        listScalar->setPrecision(0);

        auto listTheme = UI::ButtonComp::create(
            "Theme tokens",
            [title]() { title->setName("List widget: Theme tokens"); },
            {
                .sceneState = ctx.sceneState,
                .style = UI::UIElementStyle{
                    .margin = Core::Style::Margin::onlyBottom(2.f),
                },
            });

        auto listScroll = UI::ButtonComp::create(
            "Scissor viewport",
            [title]() { title->setName("List widget: Scissor viewport"); },
            {
                .sceneState = ctx.sceneState,
                .style = UI::UIElementStyle{
                    .margin = Core::Style::Margin::onlyBottom(2.f),
                },
            });

        auto listFocus = UI::ButtonComp::create(
            "Keyboard focus",
            [title]() { title->setName("List widget: Keyboard focus"); },
            {
                .sceneState = ctx.sceneState,
            });

        demoList = UI::ListBoxComp::create(
            {
                .sceneState = ctx.sceneState,
                .children =
                    {
                        listInput,
                        listOutput,
                        listDiagnostics,
                        listScalar,
                        listTheme,
                        listScroll,
                        listFocus,
                    },
                .style = UI::UIElementStyle{
                    .margin = Core::Style::Margin::onlyTop(4.f),
                },
            });
        demoList->setListSize({150.f, 88.f});

        auto tree = UI::TreeNodeComp::create(
            "Advanced controls",
            true,
            nullptr,
            {
                .sceneState = ctx.sceneState,
                .children =
                    {
                        dropdown,
                        menu,
                        selectable,
                        imageMode,
                        demoImage,
                        demoList,
                    },
                .style = UI::UIElementStyle{
                    .margin = Core::Style::Margin::onlyBottom(4.f),
                },
            });

        demoProgress = UI::ProgressBarComp::create(
            "Progress",
            kInitialDemoValue,
            0.f,
            100.f,
            {
                .sceneState = ctx.sceneState,
                .style = UI::UIElementStyle{
                    .margin = Core::Style::Margin::onlyBottom(4.f),
                },
            });
        demoProgress->setBarSize({150.f, 10.f});
        demoProgress->setValuePrecision(0);

        demoScalar = UI::ScalarInputComp::create(
            kInitialDemoValue,
            [](double value) {
                if (demoSlider) {
                    demoSlider->setValue(static_cast<float>(value));
                }
                if (demoProgress) {
                    demoProgress->setValue(static_cast<float>(value));
                }
            },
            {
                .sceneState = ctx.sceneState,
                .style = UI::UIElementStyle{
                    .margin = Core::Style::Margin::onlyBottom(4.f),
                },
            });
        demoScalar->setValueRange(0.0, 100.0);
        demoScalar->setStep(1.0);
        demoScalar->setPrecision(0);
        demoScalar->setInputSize({150.f, 22.f});

        demoSlider = UI::SliderComp::create(
            "Output", kInitialDemoValue, 0.f, 100.f, [](float value) {
                if (demoProgress) {
                    demoProgress->setValue(value);
                }
                if (demoScalar) {
                    demoScalar->setValue(value);
                }
            },
            {
                .sceneState = ctx.sceneState,
            });
        demoSlider->setStep(1.f);
        demoSlider->setValuePrecision(0);
        demoSlider->setSliderSize({150.f, 0.f});

        auto showValues = UI::CheckboxComp::create(
            "Show values",
            [](bool checked) {
                if (demoSlider) {
                    demoSlider->setShowValue(checked);
                }
                if (demoProgress) {
                    demoProgress->setShowValue(checked);
                }
            },
            true,
            {
                .sceneState = ctx.sceneState,
            });

        auto compactLabels = UI::CheckboxComp::create(
            "Compact labels",
            [](bool checked) {
                if (demoSlider) {
                    demoSlider->setShowLabel(!checked);
                }
                if (demoProgress) {
                    demoProgress->setShowLabel(!checked);
                }
            },
            false,
            {
                .sceneState = ctx.sceneState,
            });

        auto reset = UI::ButtonComp::create(
            "Reset",
            []() {
                if (demoSlider) {
                    demoSlider->setValue(kInitialDemoValue);
                }
                if (demoProgress) {
                    demoProgress->setValue(kInitialDemoValue);
                }
                if (demoScalar) {
                    demoScalar->setValue(kInitialDemoValue);
                }
            },
            {
                .sceneState = ctx.sceneState,
                .style = UI::UIElementStyle{
                    .margin = Core::Style::Margin::onlyTop(4.f),
                },
            });

        demoPanel = UI::ContainerComp::create(
            UI::LayoutDirection::vertical,
            {
                .sceneState = ctx.sceneState,
                .children =
                    {
                        title,
                        tree,
                        demoProgress,
                        demoScalar,
                        demoSlider,
                        showValues,
                        compactLabels,
                        reset,
                    },
                .style = UI::UIElementStyle{
                    .padding =
                        Core::Style::Padding(10.f, 12.f, 10.f, 12.f),
                    .margin = Core::Style::Margin(0.f),
                },
            });
        demoPanel->setName("Scratch Retained UI");
        demoPanel->setPosition({-260.f, 160.f, 1.f});
        demoPanel->setDrawBackground(true);
        demoPanel->setCrossAxisAlignment(UI::LayoutAlignment::start);
    }

    void ScratchLayer::update(TimeMs ts, SceneUpdateContext &ctx) {
    }

    void ScratchLayer::draw(SceneRenderContext &ctx) {
    }
} // namespace Bess::Canvas
