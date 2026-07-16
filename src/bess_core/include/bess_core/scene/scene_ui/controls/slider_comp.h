#pragma once

#include "common/bess_api.h"

#include "bess_core/scene/scene_ui/ui_scene_component.h"
#include <functional>
#include <memory>
#include <string>

namespace Bess::Canvas::UI {

    using UISliderCallback = std::function<void(float)>;

    class BESS_API SliderComp : public UISceneComponent {
      public:
        DEFAULT_CONTRS(SliderComp)

        [[nodiscard]] float getValue() const;
        void setValue(float value);
        [[nodiscard]] float getMinValue() const;
        void setMinValue(float value);
        [[nodiscard]] float getMaxValue() const;
        void setMaxValue(float value);
        void setValueRange(float minValue, float maxValue);
        [[nodiscard]] float getStep() const;
        void setStep(float step);

        MAKE_GETTER_SETTER_WC(bool, ShowLabel, m_showLabel, makeUIDirty)
        MAKE_GETTER_SETTER_WC(bool, ShowValue, m_showValue, makeUIDirty)
        MAKE_GETTER_SETTER_WC(glm::vec2, SliderSize, m_sliderSize, makeUIDirty)
        MAKE_GETTER_SETTER_WC(float,
                              LabelTrackSpacing,
                              m_labelTrackSpacing,
                              makeUIDirty)
        MAKE_GETTER_SETTER_WC(float,
                              ValueTrackSpacing,
                              m_valueTrackSpacing,
                              makeUIDirty)
        MAKE_GETTER_SETTER_WC(float, TrackHeight, m_trackHeight, makeUIDirty)
        MAKE_GETTER_SETTER_WC(float, ThumbRadius, m_thumbRadius, makeUIDirty)
        MAKE_GETTER_SETTER_WC(int,
                              ValuePrecision,
                              m_valuePrecision,
                              onValueFormatChanged)
        MAKE_GETTER_SETTER(UISliderCallback, ChangedCallback, m_changedCallback)

        static std::shared_ptr<SliderComp>
        create(const CompConfig &config);
        static std::shared_ptr<SliderComp>
        create(const std::string &label,
               float value,
               float minValue,
               float maxValue,
               const UISliderCallback &changedCallback = nullptr,
               const CompConfig &config = CompConfig{});

        void onDraw(SceneDrawContext &state) override;
        void prepareUI(SceneUIPrepareCtx &state) override;
        bool onMouseButton(const Events::MouseButtonEvent &e) override;
        bool onPointerMove(const Events::MouseMoveEvent &e) override;
        bool hasPointerCapture() const override;
        bool isFocusable() const override;
        bool wantsKeyboardInput() const override;
        bool onKeyEvent(const SceneEvent &evt) override;
        Core::Viewport::SceneCursor getCursor() const override;

      private:
        void prepStyle(
            const std::shared_ptr<Core::Style::BessTheme> &theme) override;

        [[nodiscard]] float sanitizeValue(float value, float fallback) const;
        [[nodiscard]] float sanitizeStep(float step) const;
        [[nodiscard]] float snappedValue(float value) const;
        [[nodiscard]] float valueToNormalized() const;
        [[nodiscard]] float pointerToValue(const glm::vec2 &pointerPos) const;
        [[nodiscard]] glm::vec2 resolveTrackSize() const;

        void setValueFromUser(float value);
        void applyKeyboardDelta(float delta);
        void normalizeRange();
        void onValueFormatChanged();
        void updateCachedValueLabel();

        float m_value = 0.f;
        float m_minValue = 0.f;
        float m_maxValue = 1.f;
        float m_step = 0.f;
        bool m_showLabel = true;
        bool m_showValue = true;
        glm::vec2 m_sliderSize{120.f, 0.f};
        float m_labelTrackSpacing = 8.f;
        float m_valueTrackSpacing = 8.f;
        float m_trackHeight = 4.f;
        float m_thumbRadius = 6.f;
        int m_valuePrecision = 2;
        bool m_dragging = false;
        UINode *m_labelNode = nullptr;
        UINode *m_trackNode = nullptr;
        UINode *m_valueNode = nullptr;
        UISliderCallback m_changedCallback;
        std::string m_cachedValueLabel;
        Color m_trackColor;
        Color m_fillColor;
        Color m_thumbColor;
    };
} // namespace Bess::Canvas::UI
