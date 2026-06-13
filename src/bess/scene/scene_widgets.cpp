#include "scene_widgets.h"
#include "scene/scene_draw_helpers.h"
#include "settings/viewport_theme.h"
#include <optional>
#include <unordered_set>

namespace Bess::Canvas::SceneWidgets {
    namespace {
        struct SceneWidgetsState {
            std::unordered_set<uint64_t> registeredWidgets;
            std::optional<uint64_t> clickedWidget;
        };

        SceneWidgetsState &state() {
            static SceneWidgetsState state;
            return state;
        }

        bool consumeClick(const PickingId &id) {
            auto &widgetsState = state();
            if (!widgetsState.clickedWidget ||
                *widgetsState.clickedWidget != id.toUint64()) {
                return false;
            }

            widgetsState.clickedWidget.reset();
            return true;
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
    } // namespace

    void beginFrame() { state().registeredWidgets.clear(); }

    void endFrame() { state().clickedWidget.reset(); }

    void registerWidget(const PickingId &id) {
        if (id.isValid()) {
            state().registeredWidgets.insert(id.toUint64());
        }
    }

    bool contains(const PickingId &id) {
        return id.isValid() &&
               state().registeredWidgets.contains(id.toUint64());
    }

    void queueClick(const PickingId &id) {
        if (contains(id)) {
            state().clickedWidget = id.toUint64();
        }
    }

    bool toggleButton(const PickingId &id, bool value,
                      const glm::vec3 &buttonPos, const glm::vec2 &buttonSize,
                      SceneDrawContext &context) {
        registerWidget(id);
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
} // namespace Bess::Canvas::SceneWidgets
