#pragma once

#include "bess_core/scene/scene_state/scene_state.h"
#include "bess_core/scene/scene_ui/text_box_context.h"
#include "bess_core/scene/scene_ui/ui_scene_component.h"
#include <cstddef>
#include <functional>
#include <memory>
#include <string>

namespace Bess::Canvas::UI {

    using UIScalarInputCallback = std::function<void(double)>;

    class ScalarInputComp : public UISceneComponent {
      public:
        DEFAULT_CONTRS(ScalarInputComp)

        static std::shared_ptr<ScalarInputComp>
        create(double value = 0.0,
               const UIScalarInputCallback &changedCallback = nullptr);

        [[nodiscard]] double getValue() const;
        void setValue(double value);
        [[nodiscard]] double getMinValue() const;
        void setMinValue(double value);
        [[nodiscard]] double getMaxValue() const;
        void setMaxValue(double value);
        void setValueRange(double minValue, double maxValue);
        [[nodiscard]] double getStep() const;
        void setStep(double step);
        [[nodiscard]] int getPrecision() const;
        void setPrecision(int precision);
        [[nodiscard]] bool getClampToRange() const;
        void setClampToRange(bool clampToRange);

        MAKE_GETTER_SETTER_WC(std::string,
                              Placeholder,
                              m_placeholder,
                              makeUIDirty)
        MAKE_GETTER_SETTER_WC(glm::vec2,
                              InputSize,
                              m_inputSize,
                              makeUIDirty)
        MAKE_GETTER_SETTER_WC(size_t, MaxLength, m_maxLength, makeUIDirty)
        MAKE_GETTER_SETTER(UIScalarInputCallback,
                           ChangedCallback,
                           m_changedCallback)

        void update(TimeMs ts, SceneState &state) override;
        void onDraw(SceneDrawContext &state) override;
        void prepareUI(SceneUIPrepareCtx &state) override;
        bool isFocusable() const override;
        bool wantsKeyboardInput() const override;
        void onFocusGained(const Events::FocusEvent &e) override;
        void onFocusLost(const Events::FocusEvent &e) override;
        bool onMouseButton(const Events::MouseButtonEvent &e) override;
        bool onPointerMove(const Events::MouseMoveEvent &e) override;
        bool onKeyEvent(const SceneEvent &evt) override;
        bool hasPointerCapture() const override;
        Core::Viewport::SceneCursor getCursor() const override;

      private:
        [[nodiscard]] double sanitizeValue(double value, double fallback) const;
        [[nodiscard]] double sanitizeStep(double step) const;
        [[nodiscard]] double normalizedValue(double value) const;
        [[nodiscard]] glm::vec2 resolveInputSize(SceneUIPrepareCtx &state) const;
        [[nodiscard]] glm::vec2 stylePadding() const;

        void normalizeRange();
        void syncTextFromValue(bool updateFocusedContext);
        void setValueFromUser(double value, bool commitDisplayText);
        void commitText();
        void restoreValidText();
        void applyTextInputResult(const TextBoxContextResult &result);
        void invalidateTextLayout();

        double m_value = 0.0;
        double m_minValue = 0.0;
        double m_maxValue = 1.0;
        double m_step = 1.0;
        bool m_clampToRange = false;
        int m_precision = 2;
        std::string m_placeholder = "0";
        std::string m_text = "0";
        glm::vec2 m_inputSize{72.f, 0.f};
        size_t m_maxLength = 64;
        UIScalarInputCallback m_changedCallback;
        TextBoxContext m_textInput;
        bool m_clearFocus = false;
    };
} // namespace Bess::Canvas::UI
