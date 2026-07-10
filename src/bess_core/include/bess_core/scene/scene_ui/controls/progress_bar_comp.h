#pragma once

#include "bess_core/scene/scene_ui/ui_scene_component.h"
#include <memory>
#include <string>

namespace Bess::Canvas::UI {

    class ProgressBarComp : public UISceneComponent {
      public:
        DEFAULT_CONTRS(ProgressBarComp)

        [[nodiscard]] float getValue() const;
        void setValue(float value);
        [[nodiscard]] float getMinValue() const;
        void setMinValue(float value);
        [[nodiscard]] float getMaxValue() const;
        void setMaxValue(float value);
        void setValueRange(float minValue, float maxValue);

        MAKE_GETTER_SETTER_WC(bool, ShowLabel, m_showLabel, makeUIDirty)
        MAKE_GETTER_SETTER_WC(bool, ShowValue, m_showValue, makeUIDirty)
        MAKE_GETTER_SETTER_WC(glm::vec2, BarSize, m_barSize, makeUIDirty)
        MAKE_GETTER_SETTER_WC(float,
                              LabelBarSpacing,
                              m_labelBarSpacing,
                              makeUIDirty)
        MAKE_GETTER_SETTER_WC(float,
                              ValueBarSpacing,
                              m_valueBarSpacing,
                              makeUIDirty)
        MAKE_GETTER_SETTER_WC(int,
                              ValuePrecision,
                              m_valuePrecision,
                              onValueFormatChanged)

        static std::shared_ptr<ProgressBarComp> create(const std::string &label,
                                                       float value,
                                                       float minValue = 0.f,
                                                       float maxValue = 1.f);

        void onDraw(SceneDrawContext &state) override;
        void prepareUI(SceneUIPrepareCtx &state) override;
        Core::Viewport::SceneCursor getCursor() const override;

      private:
        void prepStyle(
            const std::shared_ptr<Core::Style::BessTheme> &theme) override;

        [[nodiscard]] float sanitizeValue(float value, float fallback) const;
        [[nodiscard]] float valueToNormalized() const;
        void normalizeRange();
        void onValueFormatChanged();
        void updateCachedValueLabel();

        float m_value = 0.f;
        float m_minValue = 0.f;
        float m_maxValue = 1.f;
        bool m_showLabel = true;
        bool m_showValue = true;
        glm::vec2 m_barSize{120.f, 10.f};
        float m_labelBarSpacing = 8.f;
        float m_valueBarSpacing = 8.f;
        int m_valuePrecision = 0;
        UINode *m_labelNode = nullptr;
        UINode *m_barNode = nullptr;
        UINode *m_valueNode = nullptr;
        std::string m_cachedValueLabel;
        Color m_trackColor;
        Color m_fillColor;
    };
} // namespace Bess::Canvas::UI
