#include "scene_widgets.h"
#include "bess_core/renderer/colors.h"
#include "bess_core/renderer/renderer_types.h"
#include "common/bess_assert.h"
#include "common/logger.h"
#include "common/types.h"
#include "scene/scene_draw_helpers.h"
#include "settings/viewport_theme.h"
#include <cstdint>
#include <unordered_set>

namespace Bess::Canvas::SceneWidgets {
    namespace {
        struct WidgetState {
            enum class Type : uint8_t {
                toggleButton,
                button,
                textInput,
            } type;

            union WidgetData {
                struct ToggleButton {
                    bool value;
                } toggleButton;

                struct Button {
                    std::string_view label;
                } button;

                struct TextInput {
                    std::string_view text;
                    size_t maxLength;
                    size_t carretPos;
                } textInput;

                WidgetData() {}
                ~WidgetData() {}
            } data;

            bool isHovered = false;
            bool isClicked = false;
            bool isFocused = false;
        };

        struct SceneWidgetsState {
            std::unordered_set<uint64_t> registeredWidgets;
            std::unordered_map<uint64_t, WidgetState> widgetStates;
            uint64_t hoveredWidgetId = PickingId::invalid().toUint64();
        };

        SceneWidgetsState &state() {
            static SceneWidgetsState state;
            return state;
        }

        WidgetState *getWidgetState(const PickingId &id) {
            auto &widgetsState = state();
            auto it = widgetsState.widgetStates.find(id.toUint64());
            if (it == widgetsState.widgetStates.end()) {
                return nullptr;
            }
            return &it->second;
        }

        bool consumeClick(const PickingId &id) {
            auto state = getWidgetState(id);

            if (state == nullptr) {
                BESS_WARN("[SceneWidgets] Trying to consume click for "
                          "unregistered widget with id {}",
                          (uint64_t)id);
                return false;
            }

            if (state->isClicked) {
                state->isClicked = false;
                return true;
            }

            return false;
        }

        void drawToggleButton(const PickingId &id, bool isHigh,
                              const glm::vec3 &buttonPos,
                              const glm::vec2 &buttonSize,
                              SceneDrawContext &context) {
            static const SceneDraw::QuadStyle trackProps{
                .borderColor = ViewportTheme::colors.componentBorder,
                .borderRadius = glm::vec4(5.5f),
                .borderSize = glm::vec4(0.5f),
            };
            constexpr SceneDraw::QuadStyle buttonProps{.borderRadius =
                                                           glm::vec4(5.f)};

            SceneDraw::drawQuad(context, buttonPos, buttonSize,
                                isHigh ? ViewportTheme::colors.stateHigh
                                       : ViewportTheme::colors.background,
                                id, trackProps);

            const float buttonHeadPosX =
                isHigh
                    ? buttonPos.x + (buttonSize.x / 2.f) - (buttonSize.y / 2.f)
                    : buttonPos.x - (buttonSize.x / 2.f) + (buttonSize.y / 2.f);

            const glm::vec3 buttonHeadPos =
                glm::vec3(buttonHeadPosX, buttonPos.y, buttonPos.z);
            SceneDraw::drawQuad(context, buttonHeadPos,
                                {buttonSize.y - 1.f, buttonSize.y - 1.f},
                                ViewportTheme::colors.stateLow, id,
                                buttonProps);
        }

        bool isHovering(const PickingId &id) {
            return state().hoveredWidgetId == id.toUint64();
        }

        void resetWidgetsState() {
            for (auto &[id, widgetState] : state().widgetStates) {
                widgetState = {};
            }
        }

        void clearWidgetsState() { state().registeredWidgets.clear(); }

        void registerWidget(const PickingId &id,
                            SceneWidgets::WidgetState::Type type,
                            const WidgetState::WidgetData &data) {
            if (!id.isValid()) {
                return;
            }
            auto &widgetsState = state();
            widgetsState.registeredWidgets.insert(id.toUint64());

            auto it = widgetsState.widgetStates.find(id.toUint64());
            if (it != widgetsState.widgetStates.end()) {
                it->second.data = data;
            } else {
                SceneWidgets::WidgetState state{
                    .type = type,
                    .data = data,
                };
                widgetsState.widgetStates[id.toUint64()] = state;
            }
        }
    } // namespace

    void beginFrame() { clearWidgetsState(); }

    void endFrame() { resetWidgetsState(); }

    bool contains(const PickingId &id) {
        return id.isValid() &&
               state().registeredWidgets.contains(id.toUint64());
    }

    void queueClick(const PickingId &id) {
        auto state = getWidgetState(id);

        if (!state) {
            BESS_WARN("[SceneWidgets] Trying to queue click for "
                      "unregistered widget");
            return;
        }

        state->isClicked = true;
    }

    void setHoverId(const PickingId &id) {
        if (state().hoveredWidgetId == id.toUint64()) {
            return;
        }

        if (state().hoveredWidgetId != PickingId::invalid().toUint64()) {
            auto prevState = getWidgetState(PickingId(state().hoveredWidgetId));
            if (prevState) {
                prevState->isHovered = false;
            }
        }

        state().hoveredWidgetId = id.toUint64();

        if (id.isValid()) {
            auto newState = getWidgetState(id);
            if (newState) {
                newState->isHovered = true;
            }
        }
    }

    bool toggleButton(const PickingId &id, bool value,
                      const glm::vec3 &buttonPos, const glm::vec2 &buttonSize,
                      SceneDrawContext &context) {

        WidgetState::WidgetData data;
        data.toggleButton.value = value;

        registerWidget(id, WidgetState::Type::toggleButton, data);
        drawToggleButton(id, value, buttonPos, buttonSize, context);
        return consumeClick(id);
    }

    bool toggleButton(const PickingId &id, bool *value,
                      const glm::vec3 &buttonPos, const glm::vec2 &buttonSize,
                      SceneDrawContext &context) {
        if (value == nullptr) {
            return false;
        }

        const bool clicked =
            toggleButton(id, *value, buttonPos, buttonSize, context);
        if (clicked) {
            *value = !*value;
        }
        return clicked;
    }

    bool button(const PickingId &id, const std::string &label,
                const glm::vec3 &buttonPos, const glm::vec2 &buttonSize,
                const Core::Renderer::Color &labelColor,
                SceneDrawContext &context) {

        constexpr float paddingY = 2.f;
        constexpr float paddingX = 3.f;
        constexpr float defaultTextSize = 8.f;

        WidgetState::WidgetData data;
        data.button.label = label;
        registerWidget(id, WidgetState::Type::button, data);

        static const SceneDraw::QuadStyle buttonProps{
            .borderColor = Core::Renderer::Colors::slate700,
            .borderRadius = glm::vec4(2.f),
            .borderSize = glm::vec4(0.5f),
        };

        const auto bgColor = isHovering(id)
                                 ? Core::Renderer::Colors::slate900 * 1.2f
                                 : Core::Renderer::Colors::slate900;

        const auto textSize =
            context.renderer->measureText(label, {.fontSize = defaultTextSize});

        const float textOffY = context.renderer->textCenterOffsetY(
            label, {.fontSize = defaultTextSize});

        auto size = buttonSize;

        if (size.y == 0.f) {
            size.y = textSize.y + (paddingY * 2.f);
        }

        if (size.x == 0.f) {
            size.x = textSize.x + (paddingX * 2.f);
        }

        SceneDraw::drawQuad(context, buttonPos, size, bgColor, id, buttonProps);

        const auto textPos =
            glm::vec3(buttonPos.x - (textSize.x / 2.f), buttonPos.y + textOffY,
                      buttonPos.z + 0.001f);
        SceneDraw::drawText(context, label, textPos, defaultTextSize,
                            labelColor, id);

        return consumeClick(id);
    }
} // namespace Bess::Canvas::SceneWidgets
