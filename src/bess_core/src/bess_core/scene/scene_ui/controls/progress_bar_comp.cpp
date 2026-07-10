#include "bess_core/scene/scene_ui/controls/progress_bar_comp.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <limits>

namespace Bess::Canvas::UI {
    namespace {
        constexpr float kMinBarWidth = 24.f;
        constexpr float kMinBarHeight = 3.f;

        [[nodiscard]] bool isFinite(float value) {
            return std::isfinite(value);
        }

        [[nodiscard]] bool nearlyEqual(float lhs, float rhs, float range) {
            const float epsilon =
                std::max(std::numeric_limits<float>::epsilon() * 8.f,
                         std::max(1.f, range) * 0.000001f);
            return std::abs(lhs - rhs) <= epsilon;
        }

        [[nodiscard]] std::string formatPercent(float normalized,
                                                int precision) {
            std::array<char, 64> buffer{};
            const float percent = std::clamp(normalized, 0.f, 1.f) * 100.f;
            std::to_chars_result result{};
            if (precision <= 0) {
                result = std::to_chars(
                    buffer.data(),
                    buffer.data() + buffer.size(),
                    static_cast<long long>(std::llround(percent)));
            } else {
                result = std::to_chars(buffer.data(),
                                       buffer.data() + buffer.size(),
                                       percent,
                                       std::chars_format::fixed,
                                       precision);
            }

            if (result.ec != std::errc{}) {
                return "0%";
            }

            std::string label{buffer.data(), result.ptr};
            label += "%";
            return label;
        }

        void drawNodeText(SceneDrawContext &state,
                          UINode *node,
                          const std::string &text,
                          const Core::Style::ElementStyle &style) {
            if (node == nullptr || state.renderer == nullptr) {
                return;
            }

            const auto offsetY = state.renderer->textCenterOffsetY(
                text,
                {
                    .fontSize = style.textStyle.fontSize,
                });

            const auto pos = node->getDrawPos();
            const glm::vec2 textPos{
                pos.x - (node->getDrawSize().x * 0.5f),
                pos.y + offsetY,
            };

            state.renderer->drawFont(text,
                                     {
                                         .position = textPos,
                                         .fontSize = style.textStyle.fontSize,
                                         .color = style.textStyle.textColor,
                                         .zIndex = pos.z,
                                         .id = PickingId::invalid(),
                                         .transformMode = state.transformMode,
                                     });
        }
    } // namespace

    std::shared_ptr<ProgressBarComp> ProgressBarComp::create(
        const std::string &label, float value, float minValue, float maxValue) {
        auto progress = std::make_shared<ProgressBarComp>();
        progress->setName(label);
        progress->setValueRange(minValue, maxValue);
        progress->setValue(value);
        return progress;
    }

    float ProgressBarComp::getValue() const {
        return m_value;
    }

    void ProgressBarComp::setValue(float value) {
        const float range = m_maxValue - m_minValue;
        const float next = std::clamp(
            sanitizeValue(value, m_minValue), m_minValue, m_maxValue);
        if (nearlyEqual(m_value, next, range)) {
            return;
        }

        m_value = next;
        updateCachedValueLabel();
    }

    float ProgressBarComp::getMinValue() const {
        return m_minValue;
    }

    void ProgressBarComp::setMinValue(float value) {
        setValueRange(value, m_maxValue);
    }

    float ProgressBarComp::getMaxValue() const {
        return m_maxValue;
    }

    void ProgressBarComp::setMaxValue(float value) {
        setValueRange(m_minValue, value);
    }

    void ProgressBarComp::setValueRange(float minValue, float maxValue) {
        m_minValue = sanitizeValue(minValue, 0.f);
        m_maxValue = sanitizeValue(maxValue, m_minValue + 1.f);
        normalizeRange();
        setValue(m_value);
        updateCachedValueLabel();
    }

    void ProgressBarComp::onDraw(SceneDrawContext &state) {
        if (m_barNode == nullptr || state.renderer == nullptr) {
            return;
        }

        if (m_showLabel) {
            drawNodeText(state, m_labelNode, m_name, m_style);
        }
        if (m_showValue) {
            drawNodeText(state, m_valueNode, m_cachedValueLabel, m_style);
        }

        const auto barPos = m_barNode->getDrawPos();
        const auto barSize = m_barNode->getDrawSize();
        const float normalized = valueToNormalized();

        Core::Renderer::QuadProps trackProps;
        trackProps.position = barPos;
        trackProps.size = barSize;
        trackProps.zIndex = barPos.z;
        trackProps.color = m_trackColor;
        trackProps.borderColor = m_style.borderColor;
        trackProps.thickness = m_style.metrics.borderSize.toVec4();
        trackProps.radius = m_style.metrics.borderRadius;
        trackProps.id = PickingId::invalid();
        trackProps.transformMode = state.transformMode;
        state.renderer->drawQuad(trackProps);

        const float fillWidth = barSize.x * normalized;
        if (fillWidth <= 0.f) {
            return;
        }

        Core::Renderer::QuadProps fillProps = trackProps;
        fillProps.position.x =
            barPos.x - (barSize.x * 0.5f) + (fillWidth * 0.5f);
        fillProps.size.x = fillWidth;
        fillProps.zIndex = barPos.z + 0.0001f;
        fillProps.color = m_fillColor;
        fillProps.borderColor = Core::Renderer::Color{0.f, 0.f, 0.f, 0.f};
        fillProps.thickness = glm::vec4(0.f);
        state.renderer->drawQuad(fillProps);
    }

    void ProgressBarComp::prepareUI(SceneUIPrepareCtx &state) {
        prepStyle(state.theme);
        initNode(state.sceneState->getUINodeRegistry());

        if (m_labelNode == nullptr) {
            m_labelNode =
                state.sceneState->getUINodeRegistry()->addNode(UUID());
        }
        if (m_barNode == nullptr) {
            m_barNode = state.sceneState->getUINodeRegistry()->addNode(UUID());
        }
        if (m_valueNode == nullptr) {
            m_valueNode =
                state.sceneState->getUINodeRegistry()->addNode(UUID());
        }

        m_node->setDirection(LayoutDirection::horizontal);
        m_node->setWidthFitContent();
        m_node->setHeightFitContent();
        m_node->setCrossAxisAlignment(LayoutAlignment::center);
        m_node->setPadding(m_style.metrics.padding);
        m_node->setMargin(m_style.metrics.margin);

        if (m_showLabel) {
            const auto labelSize = state.renderer->measureText(
                m_name,
                {
                    .fontSize = m_style.textStyle.fontSize,
                });

            m_labelNode->setWidth(labelSize.x);
            m_labelNode->setHeight(labelSize.y);
            m_labelNode->setPosMode(PosMode::relative);
            m_labelNode->setPadding(0.f);
            m_labelNode->setMargin(Core::Style::Margin::onlyRight(
                std::max(0.f, m_labelBarSpacing)));
            m_node->addChild(m_labelNode);
        }

        m_barNode->setWidth(std::max(kMinBarWidth, m_barSize.x));
        m_barNode->setHeight(std::max(kMinBarHeight, m_barSize.y));
        m_barNode->setPosMode(PosMode::relative);
        m_barNode->setPadding(0.f);
        m_barNode->setMargin(m_showValue ? Core::Style::Margin::onlyRight(
                                               std::max(0.f, m_valueBarSpacing))
                                         : Core::Style::Margin(0.f));
        m_node->addChild(m_barNode);

        if (m_showValue) {
            const auto valueSize = state.renderer->measureText(
                formatPercent(1.f, std::clamp(m_valuePrecision, 0, 6)),
                {
                    .fontSize = m_style.textStyle.fontSize,
                });

            m_valueNode->setWidth(valueSize.x);
            m_valueNode->setHeight(valueSize.y);
            m_valueNode->setPosMode(PosMode::relative);
            m_valueNode->setPadding(0.f);
            m_valueNode->setMargin(0.f);
            m_node->addChild(m_valueNode);
        }

        if (state.parentNode != nullptr) {
            state.parentNode->addChild(m_node);
        }

        m_isUIDirty = false;
    }

    void ProgressBarComp::prepStyle(
        const std::shared_ptr<Core::Style::BessTheme> &theme) {
        UISceneComponent::prepStyle(theme);

        const auto &colors = theme->getColorScheme().getColors();
        m_trackColor = colors.secondaryContainer;
        m_fillColor = colors.primary;
    }

    Core::Viewport::SceneCursor ProgressBarComp::getCursor() const {
        return Core::Viewport::SceneCursor::normal;
    }

    float ProgressBarComp::sanitizeValue(float value, float fallback) const {
        return isFinite(value) ? value : fallback;
    }

    float ProgressBarComp::valueToNormalized() const {
        if (m_maxValue <= m_minValue) {
            return 0.f;
        }
        return std::clamp(
            (m_value - m_minValue) / (m_maxValue - m_minValue), 0.f, 1.f);
    }

    void ProgressBarComp::normalizeRange() {
        if (m_maxValue < m_minValue) {
            std::swap(m_minValue, m_maxValue);
        }
        if (m_maxValue == m_minValue) {
            m_maxValue = m_minValue + 1.f;
        }
    }

    void ProgressBarComp::onValueFormatChanged() {
        m_valuePrecision = std::clamp(m_valuePrecision, 0, 6);
        updateCachedValueLabel();
        makeUIDirty();
    }

    void ProgressBarComp::updateCachedValueLabel() {
        m_cachedValueLabel = formatPercent(valueToNormalized(),
                                           std::clamp(m_valuePrecision, 0, 6));
    }
} // namespace Bess::Canvas::UI
