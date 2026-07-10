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

        template <typename T>
        void addPanelChild(SceneLifecycleContext &ctx,
                           const std::shared_ptr<T> &component) {
            ctx.sceneState->addComponent(component);
            ctx.sceneState->attachChild(
                demoPanel->getUuid(), component->getUuid(), false);
        }

        template <typename T>
        void addChild(SceneLifecycleContext &ctx,
                      const std::shared_ptr<UI::UISceneComponent> &parent,
                      const std::shared_ptr<T> &component) {
            ctx.sceneState->addComponent(component);
            ctx.sceneState->attachChild(
                parent->getUuid(), component->getUuid(), false);
        }
    } // namespace

    void ScratchLayer::init(SceneLifecycleContext &ctx) {
        demoPanel = UI::ContainerComp::create(UI::LayoutDirection::vertical);
        demoPanel->setName("Scratch Retained UI");
        demoPanel->setPosition({-260.f, 160.f, 1.f});
        demoPanel->setDrawBackground(true);
        demoPanel->setCrossAxisAlignment(UI::LayoutAlignment::start);
        demoPanel->getStyle().padding =
            Core::Style::Padding(10.f, 12.f, 10.f, 12.f);
        demoPanel->getStyle().margin = Core::Style::Margin(0.f);
        ctx.sceneState->addComponent(demoPanel);

        auto title = UI::LabelComp::create("Retained UI Controls");
        title->getStyle().fontSize = 10.f;
        title->getStyle().margin = Core::Style::Margin::onlyBottom(4.f);
        addPanelChild(ctx, title);

        auto tree = UI::TreeNodeComp::create("Advanced controls", true);
        tree->getStyle().margin = Core::Style::Margin::onlyBottom(4.f);
        addPanelChild(ctx, tree);

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
            });
        dropdown->setHeaderSize({150.f, 22.f});
        addChild(ctx, tree, dropdown);

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
            "Right click actions");
        menu->setTriggerSize({150.f, 22.f});
        addChild(ctx, tree, menu);

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
            true);
        selectable->setButtonSize({150.f, 22.f});
        addChild(ctx, tree, selectable);

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
            });
        imageMode->setSegmentSize({50.f, 22.f});
        addChild(ctx, tree, imageMode);

        demoImage = UI::ImageComp::create({150.f, 46.f});
        demoImage->setSourceFile("assets/images/logo/BessLogo.png");
        demoImage->setFit(UI::UIImageFit::Contain);
        demoImage->setDrawBackground(true);
        demoImage->setCornerRadius(glm::vec4(6.f));
        addChild(ctx, tree, demoImage);

        demoList = UI::ListBoxComp::create();
        demoList->setListSize({150.f, 88.f});
        demoList->getStyle().margin = Core::Style::Margin::onlyTop(4.f);
        addChild(ctx, tree, demoList);

        auto listInput = UI::SelectableButtonComp::create(
            "Inputs",
            [title](bool selected) {
                title->setName(selected ? "List widget: Inputs"
                                        : "List widget");
            },
            false);
        listInput->setButtonSize({124.f, 20.f});
        listInput->getStyle().margin = Core::Style::Margin::onlyBottom(2.f);
        addChild(ctx, demoList, listInput);

        auto listOutput = UI::SelectableButtonComp::create(
            "Outputs",
            [title](bool selected) {
                title->setName(selected ? "List widget: Outputs"
                                        : "List widget");
            },
            true);
        listOutput->setButtonSize({124.f, 20.f});
        listOutput->getStyle().margin = Core::Style::Margin::onlyBottom(2.f);
        addChild(ctx, demoList, listOutput);

        auto listDiagnostics = UI::CheckboxComp::create(
            "Diagnostics",
            [title](bool checked) {
                title->setName(checked ? "List widget: Diagnostics on"
                                       : "List widget: Diagnostics off");
            },
            true);
        listDiagnostics->getStyle().margin =
            Core::Style::Margin::onlyBottom(2.f);
        addChild(ctx, demoList, listDiagnostics);

        auto listScalar = UI::ScalarInputComp::create(
            12.0,
            [title](double value) {
                title->setName("List scalar: " + std::to_string(
                                                    static_cast<int>(value)));
            });
        listScalar->setInputSize({124.f, 20.f});
        listScalar->setValueRange(0.0, 64.0);
        listScalar->setStep(1.0);
        listScalar->setPrecision(0);
        listScalar->getStyle().margin = Core::Style::Margin::onlyBottom(2.f);
        addChild(ctx, demoList, listScalar);

        auto listTheme = UI::ButtonComp::create(
            "Theme tokens",
            [title]() { title->setName("List widget: Theme tokens"); });
        listTheme->getStyle().margin = Core::Style::Margin::onlyBottom(2.f);
        addChild(ctx, demoList, listTheme);

        auto listScroll = UI::ButtonComp::create(
            "Scissor viewport",
            [title]() { title->setName("List widget: Scissor viewport"); });
        listScroll->getStyle().margin = Core::Style::Margin::onlyBottom(2.f);
        addChild(ctx, demoList, listScroll);

        auto listFocus = UI::ButtonComp::create(
            "Keyboard focus",
            [title]() { title->setName("List widget: Keyboard focus"); });
        addChild(ctx, demoList, listFocus);

        demoProgress = UI::ProgressBarComp::create(
            "Progress", kInitialDemoValue, 0.f, 100.f);
        demoProgress->setBarSize({150.f, 10.f});
        demoProgress->setValuePrecision(0);
        demoProgress->getStyle().margin = Core::Style::Margin::onlyBottom(4.f);
        addPanelChild(ctx, demoProgress);

        demoScalar = UI::ScalarInputComp::create(
            kInitialDemoValue,
            [](double value) {
                if (demoSlider) {
                    demoSlider->setValue(static_cast<float>(value));
                }
                if (demoProgress) {
                    demoProgress->setValue(static_cast<float>(value));
                }
            });
        demoScalar->setValueRange(0.0, 100.0);
        demoScalar->setStep(1.0);
        demoScalar->setPrecision(0);
        demoScalar->setInputSize({150.f, 22.f});
        demoScalar->getStyle().margin = Core::Style::Margin::onlyBottom(4.f);
        addPanelChild(ctx, demoScalar);

        demoSlider = UI::SliderComp::create(
            "Output", kInitialDemoValue, 0.f, 100.f, [](float value) {
                if (demoProgress) {
                    demoProgress->setValue(value);
                }
                if (demoScalar) {
                    demoScalar->setValue(value);
                }
            });
        demoSlider->setStep(1.f);
        demoSlider->setValuePrecision(0);
        demoSlider->setSliderSize({150.f, 0.f});
        addPanelChild(ctx, demoSlider);

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
            true);
        addPanelChild(ctx, showValues);

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
            false);
        addPanelChild(ctx, compactLabels);

        auto reset = UI::ButtonComp::create("Reset", []() {
            if (demoSlider) {
                demoSlider->setValue(kInitialDemoValue);
            }
            if (demoProgress) {
                demoProgress->setValue(kInitialDemoValue);
            }
            if (demoScalar) {
                demoScalar->setValue(kInitialDemoValue);
            }
        });
        reset->getStyle().margin = Core::Style::Margin::onlyTop(4.f);
        addPanelChild(ctx, reset);
    }

    void ScratchLayer::update(TimeMs ts, SceneUpdateContext &ctx) {
    }

    void ScratchLayer::draw(SceneRenderContext &ctx) {
    }
} // namespace Bess::Canvas
