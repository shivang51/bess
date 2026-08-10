#include "bess_core/scene/layers/scratch_layer.h"
#include "bess_core/animator/animator.h"
#include "bess_core/g_app_context.h"
#include "bess_core/renderer/renderer_types.h"
#include "bess_core/scene/scene_ui/ui_view.h"
#include "common/types.h"
#include <string>

namespace Bess::Canvas {
    namespace {
        constexpr float kInitialDemoValue = 35.f;

        std::shared_ptr<UI::PanelComp> demoPanel = nullptr;
        std::shared_ptr<UI::SliderComp> demoSlider = nullptr;
        std::shared_ptr<UI::ProgressBarComp> demoProgress = nullptr;
        std::shared_ptr<UI::ScalarInputComp> demoScalar = nullptr;
        std::shared_ptr<UI::ImageComp> demoImage = nullptr;
        std::shared_ptr<UI::ListBoxComp> demoList = nullptr;
        float m_radius = 0.f;
        glm::vec2 m_pos = glm::vec2{0.f};
    } // namespace

    // #define UI_DEMO

    void ScratchLayer::init(SceneLifecycleContext &ctx) {

        Core::AnimDesc desc = {
            .valPtr = &m_radius,
            .start = 0.f,
            .end = 50.f,
            .duration = TimeMs(2000),
        };

        Core::AnimDesc posAnimDesc = {
            .valPtr = &m_pos,
            .start = {0.f, 0.f},
            .end = {0.f, -100.f},
            .duration = TimeMs(2000),
            // .loop = Core::Loop::loopReverse,
        };

        auto &appCtx = GAppContext::getInstance();
        auto animator = appCtx.getSubSystem<Core::Animator>();

        auto rAnim = animator->addScalar(desc);
        auto posAnim = animator->addVec(posAnimDesc);

        static bool isFirstRun = true;

        rAnim->setOnAnimFinish([posAnim]() {
            if (isFirstRun)
                posAnim->play();
            else
                posAnim->playReversed();
        });

        posAnim->setOnAnimFinish([rAnim]() {
            isFirstRun = false;
            rAnim->playReversed();
        });

        rAnim->play();

#ifdef UI_DEMO
        UI::View ui{ctx.sceneState};

        auto title =
            ui.label("Retained UI Controls",
                     UI::UIElementStyle{
                         .margin = Core::Style::Margin::onlyBottom(4.f),
                         .fontSize = 10.f,
                     });

        auto dropdown = ui.dropdown(
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

        auto menu = ui.contextMenu(
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

        auto selectable = ui.selectableButton(
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

        auto imageMode = ui.segmentedButton(
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

        demoImage = ui.image({150.f, 46.f});
        demoImage->setSourceFile("assets/images/logo/BessLogo.png");
        demoImage->setFit(UI::UIImageFit::Contain);
        demoImage->setDrawBackground(true);
        demoImage->setCornerRadius(glm::vec4(6.f));

        auto listInput = ui.selectableButton(
            "Inputs",
            [title](bool selected) {
                title->setName(selected ? "List widget: Inputs"
                                        : "List widget");
            },
            false,
            UI::UIElementStyle{
                .margin = Core::Style::Margin::onlyBottom(2.f),
            });
        listInput->setButtonSize({124.f, 20.f});

        auto listOutput = ui.selectableButton(
            "Outputs",
            [title](bool selected) {
                title->setName(selected ? "List widget: Outputs"
                                        : "List widget");
            },
            true,
            UI::UIElementStyle{
                .margin = Core::Style::Margin::onlyBottom(2.f),
            });
        listOutput->setButtonSize({124.f, 20.f});

        auto listDiagnostics = ui.checkbox(
            "Diagnostics",
            [title](bool checked) {
                title->setName(checked ? "List widget: Diagnostics on"
                                       : "List widget: Diagnostics off");
            },
            true,
            UI::UIElementStyle{
                .margin = Core::Style::Margin::onlyBottom(2.f),
            });

        auto listScalar = ui.scalarInput(
            12.0,
            [title](double value) {
                title->setName("List scalar: " +
                               std::to_string(static_cast<int>(value)));
            },
            UI::UIElementStyle{
                .margin = Core::Style::Margin::onlyBottom(2.f),
            });
        listScalar->setInputSize({124.f, 20.f});
        listScalar->setValueRange(0.0, 64.0);
        listScalar->setStep(1.0);
        listScalar->setPrecision(0);

        auto listTheme = ui.button(
            "Theme tokens",
            [title]() { title->setName("List widget: Theme tokens"); },
            UI::UIElementStyle{
                .margin = Core::Style::Margin::onlyBottom(2.f),
            });

        auto listScroll = ui.button(
            "Scissor viewport",
            [title]() { title->setName("List widget: Scissor viewport"); },
            UI::UIElementStyle{
                .margin = Core::Style::Margin::onlyBottom(2.f),
            });

        auto listFocus = ui.button("Keyboard focus", [title]() {
            title->setName("List widget: Keyboard focus");
        });

        demoList = ui.listBox({
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
            .style =
                UI::UIElementStyle{
                    .margin = Core::Style::Margin::onlyTop(4.f),
                },
        });
        demoList->setListSize({150.f, 88.f});

        auto tree = ui.treeNode(
            "Advanced controls",
            true,
            nullptr,
            {
                .children =
                    {
                        dropdown,
                        menu,
                        selectable,
                        imageMode,
                        demoImage,
                        demoList,
                    },
                .style =
                    UI::UIElementStyle{
                        .margin = Core::Style::Margin::onlyBottom(4.f),
                    },
            });

        demoProgress =
            ui.progressBar("Progress",
                           kInitialDemoValue,
                           0.f,
                           100.f,
                           UI::UIElementStyle{
                               .margin = Core::Style::Margin::onlyBottom(4.f),
                           });
        demoProgress->setBarSize({150.f, 10.f});
        demoProgress->setValuePrecision(0);

        demoScalar = ui.scalarInput(
            kInitialDemoValue,
            [](double value) {
                if (demoSlider) {
                    demoSlider->setValue(static_cast<float>(value));
                }
                if (demoProgress) {
                    demoProgress->setValue(static_cast<float>(value));
                }
            },
            UI::UIElementStyle{
                .margin = Core::Style::Margin::onlyBottom(4.f),
            });
        demoScalar->setValueRange(0.0, 100.0);
        demoScalar->setStep(1.0);
        demoScalar->setPrecision(0);
        demoScalar->setInputSize({150.f, 22.f});

        demoSlider =
            ui.slider("Output", kInitialDemoValue, 0.f, 100.f, [](float value) {
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

        auto showValues = ui.checkbox(
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

        auto compactLabels = ui.checkbox(
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

        auto reset = ui.button(
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
            UI::UIElementStyle{
                .margin = Core::Style::Margin::onlyTop(4.f),
            });

        demoPanel = ui.panel(
            "Scratch Retained UI",
            {210.f, 428.f},
            {
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
                .style =
                    UI::UIElementStyle{
                        .padding = Core::Style::Padding(10.f, 12.f, 10.f, 12.f),
                        .margin = Core::Style::Margin(0.f),
                        .shadow =
                            Core::Renderer::ShadowProps{
                                .enabled = true,
                                .offset = {0.f, 4.f},
                                .blur = 10.f,
                                .color = {0.f, 0.f, 0.f, 0.22f},
                            },
                    },
            });
        demoPanel->setPosition({-360.f, -210.f, 1.f});
        demoPanel->setMinPanelSize({190.f, 210.f});
        demoPanel->setMaxPanelSize({420.f, 720.f});
        demoPanel->setContentAlignment(UI::LayoutAlignment::start);
        demoPanel->setResizeCallback([title](const glm::vec2 &size) {
            title->setName(
                "Panel size: " + std::to_string(static_cast<int>(size.x)) +
                " x " + std::to_string(static_cast<int>(size.y)));
        });
#endif
    }

    void ScratchLayer::update(TimeMs ts, SceneUpdateContext &ctx) {
    }

    void ScratchLayer::draw(SceneRenderContext &ctx) {
        // Core::Renderer::QuadProps props;
        // props.position = m_pos;
        // props.radius = glm::vec4{m_radius};
        // props.color = Core::Renderer::Colors::pastelRed;
        // props.zIndex = 1;
        // props.size = {100.f, 100.f};
        //
        // ctx.renderer->drawQuad(props);
    }
} // namespace Bess::Canvas
