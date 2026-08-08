#include "pages/main_page/scene_components/monitor_scene_comp.h"

#include "bess_core/g_app_context.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/renderer/renderer_types.h"
#include "bess_core/scene/camera.h"
#include "bess_core/scene/scene_state/components/styles/comp_style.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include "bess_core/scene/widgets/scene_widgets.h"
#include "bess_core/settings/viewport_theme.h"
#include "ext/vector_float2.hpp"
#include "ext/vector_float3.hpp"
#include "imgui.h"
#include "pages/main_page/comp_edit.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "simulation_engine.h"
#include "ui/icons/FontAwesomeIcons_Remapped.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <format>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace {
    using Bess::Core::Renderer::Color;

    constexpr float kHeaderHeight = 22.f;
    constexpr float kTitleFontSize = 10.f;
    constexpr float kLabelFontSize = 7.f;
    constexpr float kTraceStroke = 1.6f;
    constexpr float kMinTimeScale = 0.1f;
    constexpr float kMaxTimeScale = 1000.f;
    constexpr float kMinVoltageScale = 0.1f;
    constexpr float kMaxVoltageScale = 100.f;
    constexpr double kMinTimeSpanSec = 1e-9;
    constexpr std::size_t kMaxProbeSamples = 4e4;
    constexpr uint32_t kPlotPickingInfo = 2;
    constexpr uint32_t kTracePickingInfoStart = 3;
    constexpr uint32_t kTracePickingInfoEnd = kTracePickingInfoStart + 65;
    constexpr uint32_t kFollowLatestButtonInfo = 100;
    constexpr uint32_t kLegendPickingInfoStart = 200;
    constexpr uint32_t kLegendPickingInfoCount = 128;

    struct Rect {
        float left = 0.f;
        float top = 0.f;
        float right = 0.f;
        float bottom = 0.f;

        [[nodiscard]] float width() const {
            return right - left;
        }

        [[nodiscard]] float height() const {
            return bottom - top;
        }

        [[nodiscard]] glm::vec2 center() const {
            return {(left + right) * 0.5f, (top + bottom) * 0.5f};
        }

        [[nodiscard]] glm::vec2 size() const {
            return {width(), height()};
        }

        [[nodiscard]] bool isValid() const {
            return width() > 1.f && height() > 1.f;
        }
    };

    [[nodiscard]] Rect rectFromCenter(const glm::vec2 &center,
                                      const glm::vec2 &size) {
        const glm::vec2 half = size * 0.5f;
        return {
            .left = center.x - half.x,
            .top = center.y - half.y,
            .right = center.x + half.x,
            .bottom = center.y + half.y,
        };
    }

    [[nodiscard]] Rect
    inset(const Rect &rect, float left, float top, float right, float bottom) {
        return {
            .left = rect.left + left,
            .top = rect.top + top,
            .right = rect.right - right,
            .bottom = rect.bottom - bottom,
        };
    }

    [[nodiscard]] float sanitizedScale(float value,
                                       float fallback,
                                       float minValue,
                                       float maxValue) {
        if (!std::isfinite(value)) {
            return fallback;
        }
        return std::clamp(value, minValue, maxValue);
    }

    [[nodiscard]] std::string formatTimeSeconds(double seconds) {
        const double absSeconds = std::abs(seconds);
        if (absSeconds < 1e-12) {
            return "0 s";
        }
        if (absSeconds >= 1.0) {
            return std::format("{:.2f} s", seconds);
        }
        if (absSeconds >= 1e-3) {
            return std::format("{:.2f} ms", seconds * 1e3);
        }
        if (absSeconds >= 1e-6) {
            return std::format("{:.2f} us", seconds * 1e6);
        }
        return std::format("{:.2f} ns", seconds * 1e9);
    }

    [[nodiscard]] std::string formatVoltage(float voltage) {
        if (!std::isfinite(voltage)) {
            return "--";
        }
        if (std::abs(voltage) < 1e-6f) {
            return "0 V";
        }
        const float absVoltage = std::abs(voltage);
        return absVoltage >= 10.f ? std::format("{:.1f} V", voltage)
                                  : std::format("{:.2f} V", voltage);
    }

    [[nodiscard]] double niceTickStep(double span, int targetIntervals) {
        if (!std::isfinite(span) || span <= 0.0) {
            return 1.0;
        }

        const double rawStep =
            span / static_cast<double>(std::max(targetIntervals, 1));
        const double magnitude =
            std::pow(10.0, std::floor(std::log10(rawStep)));
        const double normalized = rawStep / magnitude;

        double nice = 10.0;
        if (normalized <= 1.0) {
            nice = 1.0;
        } else if (normalized <= 2.0) {
            nice = 2.0;
        } else if (normalized <= 2.5) {
            nice = 2.5;
        } else if (normalized <= 5.0) {
            nice = 5.0;
        }

        return nice * magnitude;
    }

    [[nodiscard]] std::vector<double> makeAnchoredTicks(double minValue,
                                                        double maxValue,
                                                        int targetIntervals,
                                                        int maxTicks) {
        if (!std::isfinite(minValue) || !std::isfinite(maxValue) ||
            maxValue < minValue || maxTicks <= 0) {
            return {};
        }

        if (std::abs(maxValue - minValue) <
            std::numeric_limits<double>::epsilon()) {
            return {minValue};
        }

        const double step = niceTickStep(maxValue - minValue, targetIntervals);
        if (!std::isfinite(step) || step <= 0.0) {
            return {};
        }

        std::vector<double> ticks;
        ticks.reserve(static_cast<std::size_t>(maxTicks));

        const double epsilon = step * 1e-6;
        double value = std::ceil((minValue - epsilon) / step) * step;
        if (value < minValue - epsilon) {
            value += step;
        }

        for (int guard = 0; guard < maxTicks && value <= maxValue + epsilon;
             ++guard, value += step) {
            ticks.push_back(std::abs(value) < epsilon ? 0.0 : value);
        }

        if (ticks.empty()) {
            ticks.push_back((minValue + maxValue) * 0.5);
        }

        return ticks;
    }

    [[nodiscard]] std::string truncateLabel(const std::string &label,
                                            std::size_t maxChars) {
        if (label.size() <= maxChars) {
            return label;
        }
        if (maxChars == 0) {
            return {};
        }
        if (maxChars <= 3) {
            return label.substr(0, maxChars);
        }
        return label.substr(0, maxChars - 3) + "...";
    }

    [[nodiscard]] Color traceColorForIndex(std::size_t index) {
        static constexpr std::array<Color, 8> palette = {
            Color::fromRGBA8(80, 190, 255),
            Color::fromRGBA8(69, 214, 125),
            Color::fromRGBA8(255, 199, 77),
            Color::fromRGBA8(255, 112, 102),
            Color::fromRGBA8(191, 133, 255),
            Color::fromRGBA8(64, 222, 216),
            Color::fromRGBA8(255, 145, 77),
            Color::fromRGBA8(214, 232, 92),
        };
        return palette[index % palette.size()];
    }

    struct ProbeLinkCurve {
        glm::vec2 control1{0.f};
        glm::vec2 control2{0.f};
    };

    [[nodiscard]] ProbeLinkCurve probeLinkCurveFor(const glm::vec2 &start,
                                                   const glm::vec2 &end) {
        const glm::vec2 delta = end - start;
        const float distance = std::hypot(delta.x, delta.y);
        if (distance < 0.001f) {
            return {.control1 = start, .control2 = end};
        }

        const float xDistance = std::abs(delta.x);
        const float yDistance = std::abs(delta.y);
        if (xDistance < 12.f && yDistance > 0.f) {
            const float verticalSign = delta.y < 0.f ? -1.f : 1.f;
            const float tangent = std::clamp(yDistance * 0.42f, 20.f, 120.f);
            return {
                .control1 = start + glm::vec2(0.f, verticalSign * tangent),
                .control2 = end - glm::vec2(0.f, verticalSign * tangent),
            };
        }

        const float horizontalSign = delta.x < 0.f ? -1.f : 1.f;
        const float startTangent = std::clamp(
            std::max(xDistance * 0.48f, yDistance * 0.22f), 28.f, 180.f);
        const float endTangent = std::clamp(
            std::max(xDistance * 0.34f, distance * 0.16f), 24.f, 150.f);

        return {
            .control1 = start + glm::vec2(horizontalSign * startTangent, 0.f),
            .control2 = end - glm::vec2(horizontalSign * endTangent, 0.f),
        };
    }

    [[nodiscard]] float probeLinkLaneY(const Rect &plot,
                                       std::size_t traceIndex,
                                       std::size_t traceCount) {
        if (!plot.isValid() || traceCount <= 1) {
            return plot.center().y;
        }

        const float insetY = std::min(10.f, plot.height() * 0.18f);
        const float usableHeight = std::max(1.f, plot.height() - insetY * 2.f);
        const float lane = (static_cast<float>(traceIndex) + 0.5f) /
                           static_cast<float>(traceCount);
        return plot.top + insetY + usableHeight * lane;
    }

    void drawQuad(Bess::SceneDrawContext &context,
                  const Rect &rect,
                  float z,
                  const Color &color,
                  const Bess::PickingId &id,
                  const glm::vec4 &radius = glm::vec4(0.f),
                  const Color &borderColor = Color(0.f, 0.f, 0.f, 0.f),
                  const glm::vec4 &borderThickness = glm::vec4(0.f)) {
        if (!context.renderer || !rect.isValid()) {
            return;
        }

        Bess::Core::Renderer::QuadProps props;
        props.position = rect.center();
        props.size = rect.size();
        props.zIndex = z;
        props.color = color;
        props.id = id;
        props.radius = radius;
        props.borderColor = borderColor;
        props.thickness = borderThickness;
        props.transformMode = context.transformMode;
        context.renderer->drawQuad(props);
    }

    void drawText(Bess::SceneDrawContext &context,
                  std::string_view text,
                  const glm::vec2 &position,
                  float z,
                  float fontSize,
                  const Color &color,
                  const Bess::PickingId &id) {
        if (!context.renderer || text.empty()) {
            return;
        }

        Bess::Core::Renderer::FontProps props;
        props.position = position;
        props.fontSize = fontSize;
        props.color = color;
        props.zIndex = z;
        props.id = id;
        props.transformMode = context.transformMode;
        context.renderer->drawFont(text, props);
    }

    void drawLine(Bess::SceneDrawContext &context,
                  const glm::vec2 &start,
                  const glm::vec2 &end,
                  float z,
                  float thickness,
                  const Color &color,
                  const Bess::PickingId &id) {
        if (!context.renderer) {
            return;
        }

        Bess::Core::Renderer::LineProps props;
        props.p0 = start;
        props.p1 = end;
        props.zIndex = z;
        props.thickness = thickness;
        props.color = color;
        props.id = id;
        props.transformMode = context.transformMode;
        context.renderer->drawLine(props);
    }

    [[nodiscard]] bool pointInside(const Rect &rect, const glm::vec2 &point) {
        return point.x >= rect.left && point.x <= rect.right &&
               point.y >= rect.top && point.y <= rect.bottom;
    }

    bool clipBoundary(float p, float q, float &u0, float &u1) {
        constexpr float epsilon = 1e-6f;
        if (std::abs(p) < epsilon) {
            return q >= 0.f;
        }

        const float r = q / p;
        if (p < 0.f) {
            if (r > u1) {
                return false;
            }
            if (r > u0) {
                u0 = r;
            }
            return true;
        }

        if (r < u0) {
            return false;
        }
        if (r < u1) {
            u1 = r;
        }
        return true;
    }

    [[nodiscard]] bool
    clipLineToRect(glm::vec2 &start, glm::vec2 &end, const Rect &rect) {
        if (!rect.isValid()) {
            return false;
        }

        const glm::vec2 delta = end - start;
        float u0 = 0.f;
        float u1 = 1.f;

        if (!clipBoundary(-delta.x, start.x - rect.left, u0, u1) ||
            !clipBoundary(delta.x, rect.right - start.x, u0, u1) ||
            !clipBoundary(-delta.y, start.y - rect.top, u0, u1) ||
            !clipBoundary(delta.y, rect.bottom - start.y, u0, u1)) {
            return false;
        }

        const glm::vec2 originalStart = start;
        if (u1 < 1.f) {
            end = originalStart + (delta * u1);
        }
        if (u0 > 0.f) {
            start = originalStart + (delta * u0);
        }
        return true;
    }

    [[nodiscard]] std::string
    slotLabelFor(const Bess::Canvas::SceneState &state,
                 const Bess::UUID &slotUuid) {
        const auto slot =
            state.getComponentByUuid<Bess::Canvas::SlotSceneComponent>(
                slotUuid);
        if (!slot) {
            return "Missing slot";
        }

        std::string slotName = slot->getName();
        if (slotName.empty()) {
            slotName = std::format(
                "{} {}", slot->isInputSlot() ? "In" : "Out", slot->getIndex());
        }

        const auto parent =
            state.getComponentByUuid<Bess::Canvas::SimulationSceneComponent>(
                slot->getParentComponent());
        if (parent && !parent->getName().empty()) {
            return parent->getName() + "." + slotName;
        }

        return slotName;
    }

    [[nodiscard]] std::string latestVoltageLabel(
        const std::vector<std::pair<Bess::TimeNs, float>> &data) {
        for (auto it = data.rbegin(); it != data.rend(); ++it) {
            if (std::isfinite(it->second)) {
                return formatVoltage(it->second);
            }
        }
        return "--";
    }

    [[nodiscard]] bool isPlotPickingInfo(uint32_t details) {
        return details == kPlotPickingInfo ||
               (details >= kTracePickingInfoStart &&
                details < kTracePickingInfoEnd);
    }

} // namespace

namespace Bess::Canvas {
    struct MonitorSceneComp::PlotLayout {
        Rect outer;
        Rect frame;
        Rect plot;
        Rect lineClip;
        double visibleTimeSpanSec = 1.0;
        double maxTimePanSec = 0.0;
        double minTimeSec = 0.0;
        double maxTimeSec = 1.0;
        double timeStartSec = 0.0;
        double timeEndSec = 1.0;
        float voltageMin = -0.25f;
        float voltageMax = 5.25f;
        float signalVoltageMin = 0.f;
        float signalVoltageMax = 5.f;
        float labelFontSize = kLabelFontSize;
        bool compact = false;
        bool hasData = false;
        bool hasSignalVoltage = false;
    };

    MonitorSceneComp::MonitorSceneComp() {
        m_name = "Monitor Node";
        m_transform.scale = {380.f, 220.f};
        m_icon = ::Bess::UI::Icons::FontAwesomeIcons::FA_DISPLAY;
    }

    std::vector<std::shared_ptr<SceneComponent>>
    MonitorSceneComp::clone(const SceneState &sceneState) const {
        (void)sceneState;
        auto clonedComponent = std::make_shared<MonitorSceneComp>(*this);
        prepareClone(*clonedComponent);
        clonedComponent->m_probedSlots.clear();
        clonedComponent->m_hiddenProbedSlots.clear();
        clonedComponent->m_probeData.clear();
        clonedComponent->m_probeSourceCursors.clear();
        clonedComponent->m_probeBounds.clear();
        clonedComponent->m_isPlotHovered = false;
        clonedComponent->m_isPlotDragging = false;
        clonedComponent->m_plotDragBefore = {};
        clonedComponent->m_plotDragScene = UUID::null;
        return {clonedComponent};
    }

    void MonitorSceneComp::draw(SceneDrawContext &context) {
        if (!context.renderer || !context.sceneState) {
            return;
        }

        const auto pickingId = PickingId{m_runtimeId, 0};
        const PlotLayout layout = makePlotLayout(context);
        const float z = m_transform.position.z;
        const Color borderColor = m_isSelected
                                      ? ViewportTheme::colors.selectedComp
                                      : ViewportTheme::colors.componentBorder;

        Core::Renderer::QuadProps backgroundProps;
        backgroundProps.position = glm::vec2(m_transform.position);
        backgroundProps.size = m_transform.scale;
        backgroundProps.zIndex = z;
        backgroundProps.color = ViewportTheme::colors.componentBG.withAlpha(
            ViewportTheme::isDark ? 0.86f : 0.96f);
        backgroundProps.id = pickingId;
        backgroundProps.radius = Styles::componentStyles.borderRadius;
        backgroundProps.borderColor = borderColor;
        backgroundProps.thickness = Styles::componentStyles.borderSize;
        backgroundProps.shadow.enabled = true;
        backgroundProps.shadow.offset = {0.f, 7.f};
        backgroundProps.shadow.blur = 18.f;
        backgroundProps.shadow.spread = 1.f;
        backgroundProps.shadow.color = Color{0.f, 0.f, 0.f, 0.28f};
        backgroundProps.transformMode = context.transformMode;
        context.renderer->drawQuad(backgroundProps);

        const float headerCenterY = layout.outer.top + (kHeaderHeight * 0.5f);
        const float titleOffsetY = context.renderer->textCenterOffsetY(
            m_name, {.fontSize = kTitleFontSize});
        drawText(context,
                 m_name,
                 {layout.outer.left + Styles::componentStyles.paddingX,
                  headerCenterY + titleOffsetY},
                 z + 0.0004f,
                 kTitleFontSize,
                 ViewportTheme::colors.text,
                 pickingId);

        const std::size_t visibleCount = visibleProbeCount();
        const std::string traceSummary =
            visibleCount == m_probedSlots.size()
                ? std::format(
                      "{} trace{}  x{:.1f}",
                      m_probedSlots.size(),
                      m_probedSlots.size() == 1 ? "" : "s",
                      sanitizedScale(
                          m_timeScale, 1.f, kMinTimeScale, kMaxTimeScale))
                : std::format(
                      "{}/{} traces  x{:.1f}",
                      visibleCount,
                      m_probedSlots.size(),
                      sanitizedScale(
                          m_timeScale, 1.f, kMinTimeScale, kMaxTimeScale));
        const auto summarySize = context.renderer->measureText(
            traceSummary, {.fontSize = layout.labelFontSize});
        if (summarySize.x < layout.outer.width() -
                                (Styles::componentStyles.paddingX * 2.f) -
                                142.f) {
            const float summaryOffsetY = context.renderer->textCenterOffsetY(
                traceSummary, {.fontSize = layout.labelFontSize});
            drawText(context,
                     traceSummary,
                     {layout.outer.right - Styles::componentStyles.paddingX -
                          summarySize.x - 46.f,
                      headerCenterY + summaryOffsetY},
                     z + 0.0004f,
                     layout.labelFontSize,
                     ViewportTheme::sceneWidgetsColors.textMuted,
                     pickingId);
        }

        drawPlotFrame(context, layout);
        drawGridAndLabels(context, layout);
        plotProbedData(context, layout);
        if (!layout.hasData) {
            drawEmptyState(context, layout);
        }
        drawFollowLatestButton(context, layout);
        drawLegend(context, layout);
        drawHoverReadout(context, layout);

        std::size_t traceIndex = 0;
        for (const auto &slotUuid : m_probedSlots) {
            const auto &comp =
                context.sceneState->getComponentByUuid<SlotSceneComponent>(
                    slotUuid);
            if (!comp) {
                ++traceIndex;
                continue;
            }

            const float laneY =
                probeLinkLaneY(layout.plot, traceIndex, m_probedSlots.size());
            const glm::vec3 slotStartPos = {
                layout.frame.left, laneY, z + 0.0001f};
            auto slotPos = comp->getConnectionPos(*context.sceneState,
                                                  context.isSchematicMode);
            slotPos.z = slotStartPos.z;

            const bool hidden = isProbeHidden(slotUuid);
            const Color traceColor = traceColorForIndex(traceIndex);
            const Color linkColor =
                hidden ? traceColor.withAlpha(ViewportTheme::isDark ? 0.22f
                                                                    : 0.28f)
                       : traceColor.withAlpha(0.72f);
            const ProbeLinkCurve curve =
                probeLinkCurveFor(glm::vec2(slotStartPos.x, slotStartPos.y),
                                  glm::vec2(slotPos.x, slotPos.y));

            context.renderer->beginPath({
                .strokeColor = linkColor,
                .renderFill = false,
                .zIndex = z + 0.0001f,
                .id = PickingId{m_runtimeId, 1},
                .closePath = false,
                .transformMode = context.transformMode,
            });
            context.renderer->pathMoveTo(slotStartPos);
            context.renderer->pathCubicTo(
                curve.control1, curve.control2, slotPos, hidden ? 0.9f : 1.25f);
            context.renderer->endPath();
            ++traceIndex;
        }
    }

    MonitorSceneComp::PlotLayout
    MonitorSceneComp::makePlotLayout(SceneDrawContext &context) const {
        (void)context;
        PlotLayout layout;
        layout.outer =
            rectFromCenter(glm::vec2(m_transform.position), m_transform.scale);
        layout.compact =
            m_transform.scale.x < 300.f || m_transform.scale.y < 180.f;
        layout.labelFontSize = layout.compact ? 6.f : kLabelFontSize;

        const float padX = Styles::componentStyles.paddingX;
        const float padY = Styles::componentStyles.paddingY;
        layout.frame =
            inset(layout.outer, padX, kHeaderHeight + padY, padX, padY);

        const float axisLeft = layout.compact ? 38.f : 50.f;
        const float axisBottom = layout.compact ? 16.f : 20.f;
        const float legendTop =
            m_showLegend ? (layout.compact ? 14.f : 18.f) : 8.f;
        layout.plot = inset(layout.frame, axisLeft, legendTop, 8.f, axisBottom);
        if (!layout.plot.isValid()) {
            layout.plot = inset(layout.frame, 4.f, 4.f, 4.f, 4.f);
        }
        layout.lineClip = inset(layout.plot,
                                kTraceStroke,
                                kTraceStroke,
                                kTraceStroke,
                                kTraceStroke);

        TimeNs minTime = TimeNs::max();
        TimeNs maxTime = TimeNs::min();
        float minVoltage = std::numeric_limits<float>::max();
        float maxVoltage = std::numeric_limits<float>::lowest();
        bool hasVoltage = false;

        for (const auto &slotUuid : m_probedSlots) {
            if (isProbeHidden(slotUuid)) {
                continue;
            }

            const auto dataIt = m_probeData.find(slotUuid);
            if (dataIt == m_probeData.end() || dataIt->second.empty()) {
                continue;
            }

            minTime = std::min(minTime, dataIt->second.front().first);
            maxTime = std::max(maxTime, dataIt->second.back().first);

            const auto boundsIt = m_probeBounds.find(slotUuid);
            if (boundsIt != m_probeBounds.end() &&
                boundsIt->second.hasFiniteVoltage) {
                minVoltage = std::min(minVoltage, boundsIt->second.minVoltage);
                maxVoltage = std::max(maxVoltage, boundsIt->second.maxVoltage);
                hasVoltage = true;
            }
        }

        layout.hasData = minTime <= maxTime;
        if (layout.hasData) {
            const double minTimeSec =
                std::chrono::duration<double>(minTime).count();
            const double maxTimeSec =
                std::chrono::duration<double>(maxTime).count();
            layout.minTimeSec = minTimeSec;
            layout.maxTimeSec = maxTimeSec;
            const double dataSpan = std::max(0.0, maxTimeSec - minTimeSec);
            const float timeScale =
                sanitizedScale(m_timeScale, 1.f, kMinTimeScale, kMaxTimeScale);
            double visibleSpan = std::max(
                dataSpan > 0.0 ? dataSpan / timeScale : 1e-6, kMinTimeSpanSec);
            if (!m_followLatest && std::isfinite(m_viewTimeSpanSeconds) &&
                m_viewTimeSpanSeconds > 0.0) {
                visibleSpan = std::clamp(m_viewTimeSpanSeconds,
                                         kMinTimeSpanSec,
                                         std::max(dataSpan, kMinTimeSpanSec));
            }
            layout.visibleTimeSpanSec = visibleSpan;
            layout.maxTimePanSec = std::max(0.0, dataSpan - visibleSpan);

            if (m_followLatest || layout.maxTimePanSec <= 0.0 ||
                !std::isfinite(m_viewEndTimeSeconds)) {
                layout.timeEndSec = maxTimeSec;
            } else {
                const double minEndSec = maxTimeSec - layout.maxTimePanSec;
                layout.timeEndSec =
                    std::clamp(m_viewEndTimeSeconds, minEndSec, maxTimeSec);
            }
            layout.timeStartSec = layout.timeEndSec - visibleSpan;
            if (layout.timeEndSec <= layout.timeStartSec) {
                layout.timeEndSec = layout.timeStartSec + kMinTimeSpanSec;
            }
        }

        if (hasVoltage) {
            layout.signalVoltageMin = minVoltage;
            layout.signalVoltageMax = maxVoltage;
            layout.hasSignalVoltage = true;
        }

        const float domainMin = hasVoltage ? std::min(0.f, minVoltage) : 0.f;
        const float domainMax = hasVoltage ? std::max(5.f, maxVoltage) : 5.f;
        const float domainCenter = (domainMin + domainMax) * 0.5f;
        const float baseSpan = std::max(domainMax - domainMin, 0.25f) * 1.12f;
        const float voltageScale = sanitizedScale(
            m_voltageScale, 1.f, kMinVoltageScale, kMaxVoltageScale);
        const float visibleVoltageSpan =
            std::max(baseSpan / voltageScale, 0.05f);

        layout.voltageMin = domainCenter - (visibleVoltageSpan * 0.5f);
        layout.voltageMax = domainCenter + (visibleVoltageSpan * 0.5f);
        return layout;
    }

    void MonitorSceneComp::drawPlotFrame(SceneDrawContext &context,
                                         const PlotLayout &layout) const {
        const float z = m_transform.position.z + 0.0001f;
        const Color frameColor = ViewportTheme::isDark
                                     ? Color(0.07f, 0.075f, 0.085f, 0.96f)
                                     : Color(0.96f, 0.965f, 0.97f, 0.96f);
        const Color plotColor = ViewportTheme::isDark
                                    ? Color(0.018f, 0.021f, 0.026f, 0.98f)
                                    : Color(1.f, 1.f, 1.f, 0.98f);

        drawQuad(context,
                 layout.frame,
                 z,
                 frameColor,
                 PickingId{m_runtimeId, kPlotPickingInfo},
                 glm::vec4(4.f),
                 ViewportTheme::colors.componentBorder.withAlpha(0.8f),
                 glm::vec4(1.f));
        drawQuad(context,
                 layout.plot,
                 z + 0.0001f,
                 plotColor,
                 PickingId{m_runtimeId, kPlotPickingInfo},
                 glm::vec4(2.f));
    }

    void MonitorSceneComp::drawGridAndLabels(SceneDrawContext &context,
                                             const PlotLayout &layout) const {
        if (!layout.plot.isValid()) {
            return;
        }

        const float z = m_transform.position.z + 0.00025f;
        const Color gridColor = ViewportTheme::colors.gridMajorColor.withAlpha(
            ViewportTheme::isDark ? 0.18f : 0.55f);
        const Color axisColor =
            ViewportTheme::colors.componentBorder.withAlpha(0.86f);
        const Color labelColor = ViewportTheme::sceneWidgetsColors.textMuted;

        const auto drawPlotBounds = [&]() {
            drawLine(context,
                     {layout.plot.left, layout.plot.top},
                     {layout.plot.right, layout.plot.top},
                     z,
                     1.f,
                     axisColor,
                     PickingId{m_runtimeId, kPlotPickingInfo});
            drawLine(context,
                     {layout.plot.left, layout.plot.bottom},
                     {layout.plot.right, layout.plot.bottom},
                     z,
                     1.f,
                     axisColor,
                     PickingId{m_runtimeId, kPlotPickingInfo});
            drawLine(context,
                     {layout.plot.left, layout.plot.top},
                     {layout.plot.left, layout.plot.bottom},
                     z,
                     1.f,
                     axisColor,
                     PickingId{m_runtimeId, kPlotPickingInfo});
            drawLine(context,
                     {layout.plot.right, layout.plot.top},
                     {layout.plot.right, layout.plot.bottom},
                     z,
                     1.f,
                     axisColor,
                     PickingId{m_runtimeId, kPlotPickingInfo});
        };

        const auto voltageToY = [&](float voltage) {
            const float voltageSpan =
                std::max(layout.voltageMax - layout.voltageMin, 1e-6f);
            const float normalized = std::clamp(
                (voltage - layout.voltageMin) / voltageSpan, 0.f, 1.f);
            return layout.plot.bottom - (layout.plot.height() * normalized);
        };
        const auto timeToX = [&](double timeSec) {
            const double timeSpan = std::max(
                layout.timeEndSec - layout.timeStartSec, kMinTimeSpanSec);
            const float normalized = std::clamp(
                static_cast<float>((timeSec - layout.timeStartSec) / timeSpan),
                0.f,
                1.f);
            return layout.plot.left + (layout.plot.width() * normalized);
        };

        const float signalSpan =
            layout.signalVoltageMax - layout.signalVoltageMin;
        const bool hasSingleSignalValue =
            layout.hasSignalVoltage && std::abs(signalSpan) <= 1e-6f;
        const int yLabelCount =
            hasSingleSignalValue ? 1 : (layout.compact ? 3 : 5);
        float previousYLabelY = -std::numeric_limits<float>::max();
        for (int i = 0; i < yLabelCount; ++i) {
            const float t = yLabelCount == 1
                                ? 0.f
                                : static_cast<float>(i) /
                                      static_cast<float>(yLabelCount - 1);
            const float value = layout.signalVoltageMax - (signalSpan * t);
            const float y = voltageToY(value);
            const std::string label = formatVoltage(value);
            const auto labelSize = context.renderer->measureText(
                label, {.fontSize = layout.labelFontSize});
            const float labelSpacing = labelSize.y + 2.f;
            if (i > 0 && y < previousYLabelY + labelSpacing) {
                continue;
            }
            if (m_showGrid) {
                drawLine(context,
                         {layout.plot.left, y},
                         {layout.plot.right, y},
                         z,
                         0.55f,
                         gridColor,
                         PickingId{m_runtimeId, kPlotPickingInfo});
            }
            const float labelOffsetY = context.renderer->textCenterOffsetY(
                label, {.fontSize = layout.labelFontSize});
            const float x = std::max(layout.frame.left + 4.f,
                                     layout.plot.left - labelSize.x - 5.f);
            drawText(context,
                     label,
                     {x, y + labelOffsetY},
                     z + 0.0001f,
                     layout.labelFontSize,
                     labelColor,
                     PickingId{m_runtimeId, kPlotPickingInfo});
            previousYLabelY = y;
        }

        const float xLabelCenterY =
            layout.frame.bottom -
            ((layout.frame.bottom - layout.plot.bottom) * 0.5f);
        const int timeTargetIntervals =
            std::clamp(static_cast<int>(layout.plot.width() /
                                        (layout.compact ? 86.f : 78.f)),
                       layout.compact ? 2 : 3,
                       layout.compact ? 4 : 8);
        const std::vector<double> timeTicks =
            makeAnchoredTicks(layout.timeStartSec,
                              layout.timeEndSec,
                              timeTargetIntervals,
                              layout.compact ? 6 : 10);
        float previousXLabelRight = -std::numeric_limits<float>::max();
        for (const double timeTick : timeTicks) {
            const float tickX = timeToX(timeTick);
            const std::string label = formatTimeSeconds(timeTick);
            const auto labelSize = context.renderer->measureText(
                label, {.fontSize = layout.labelFontSize});
            const float normalizedX = std::clamp(
                (tickX - layout.plot.left) / layout.plot.width(), 0.f, 1.f);
            float x = tickX - (labelSize.x * normalizedX);
            x = std::clamp(x,
                           layout.plot.left,
                           std::max(layout.plot.left,
                                    layout.frame.right - labelSize.x - 4.f));
            if (x < previousXLabelRight + 4.f) {
                continue;
            }
            if (m_showGrid) {
                drawLine(context,
                         {tickX, layout.plot.top},
                         {tickX, layout.plot.bottom},
                         z,
                         0.55f,
                         gridColor,
                         PickingId{m_runtimeId, kPlotPickingInfo});
            }
            const float labelOffsetY = context.renderer->textCenterOffsetY(
                label, {.fontSize = layout.labelFontSize});
            drawText(context,
                     label,
                     {x, xLabelCenterY + labelOffsetY},
                     z + 0.0001f,
                     layout.labelFontSize,
                     labelColor,
                     PickingId{m_runtimeId, kPlotPickingInfo});
            previousXLabelRight = x + labelSize.x;
        }

        drawPlotBounds();
    }

    void MonitorSceneComp::update(TimeMs frameTime, SceneState &state) {
        (void)frameTime;

        if (m_probedSlots.empty()) {
            m_probeData.clear();
            m_probeSourceCursors.clear();
            m_probeBounds.clear();
            return;
        }

        const auto removeStale = [this](auto &values) {
            for (auto it = values.begin(); it != values.end();) {
                if (!m_probedSlots.contains(it->first)) {
                    values.erase(it++);
                } else {
                    ++it;
                }
            }
        };
        removeStale(m_probeData);
        removeStale(m_probeSourceCursors);
        removeStale(m_probeBounds);

        if (visibleProbeCount() == 0) {
            return;
        }

        auto *simEngine = state.runtime().sim;
        if (!simEngine) {
            m_probeData.clear();
            m_probeSourceCursors.clear();
            m_probeBounds.clear();
            return;
        }

        const auto stampData = simEngine->getStampData();
        for (const auto &slotUuid : m_probedSlots) {
            if (isProbeHidden(slotUuid)) {
                continue;
            }

            const auto slot =
                state.getComponentByUuid<SlotSceneComponent>(slotUuid);
            if (!slot || slot->getIndex() < 0) {
                m_probeData.erase(slotUuid);
                m_probeSourceCursors.erase(slotUuid);
                m_probeBounds.erase(slotUuid);
                continue;
            }

            const auto simComp =
                state.getComponentByUuid<SimulationSceneComponent>(
                    slot->getParentComponent());
            if (!simComp) {
                m_probeData.erase(slotUuid);
                m_probeSourceCursors.erase(slotUuid);
                m_probeBounds.erase(slotUuid);
                continue;
            }

            const UUID componentId = simComp->getSimEngineId();
            const auto history = stampData.find(componentId);
            if (!history || history->samples.empty()) {
                m_probeData.erase(slotUuid);
                m_probeSourceCursors.erase(slotUuid);
                m_probeBounds.erase(slotUuid);
                continue;
            }

            const auto &samples = history->samples;
            const int slotIndexValue = slot->getIndex();
            const auto slotIndex = static_cast<std::size_t>(slotIndexValue);
            const bool isInput = slot->isInputSlot();
            auto &cursor = m_probeSourceCursors[slotUuid];
            const bool sourceChanged =
                cursor.componentId != componentId ||
                cursor.slotIndex != slotIndexValue ||
                cursor.isInput != isInput ||
                cursor.generation != history->generation ||
                cursor.sourceSampleCount > samples.size() ||
                (cursor.sourceSampleCount > 0 &&
                 cursor.firstSourceTime != samples.front().simTime);
            if (!sourceChanged && cursor.revision == history->revision) {
                continue;
            }

            // An equal-sized revision means the latest timestamp was
            // replaced. Rebuild so cached values and bounds stay exact.
            const bool rebuild =
                sourceChanged || cursor.sourceSampleCount == samples.size();
            auto &probeData = m_probeData[slotUuid];
            auto &bounds = m_probeBounds[slotUuid];
            std::size_t firstSample = cursor.sourceSampleCount;
            if (rebuild) {
                probeData.clear();
                bounds = {};
                firstSample = samples.size() > kMaxProbeSamples
                                  ? samples.size() - kMaxProbeSamples
                                  : 0;
                probeData.reserve(samples.size() - firstSample);
            }

            for (std::size_t i = firstSample; i < samples.size(); ++i) {
                const auto &stamp = samples[i];
                const auto &states = slot->isInputSlot() ? stamp.inputStates
                                                         : stamp.outputStates;
                if (slotIndex >= states.size()) {
                    continue;
                }

                const float value =
                    static_cast<float>(states[slotIndex].getNumericValue());
                probeData.emplace_back(stamp.simTime, value);
                if (std::isfinite(value)) {
                    if (!bounds.hasFiniteVoltage) {
                        bounds.minVoltage = value;
                        bounds.maxVoltage = value;
                        bounds.hasFiniteVoltage = true;
                    } else {
                        bounds.minVoltage = std::min(bounds.minVoltage, value);
                        bounds.maxVoltage = std::max(bounds.maxVoltage, value);
                    }
                }
            }

            cursor = {
                .componentId = componentId,
                .firstSourceTime = samples.front().simTime,
                .sourceSampleCount = samples.size(),
                .generation = history->generation,
                .revision = history->revision,
                .slotIndex = slotIndexValue,
                .isInput = isInput,
            };
        }
    }

    void MonitorSceneComp::drawLegend(SceneDrawContext &context,
                                      const PlotLayout &layout) {
        if (!m_showLegend || m_probedSlots.empty() || !layout.frame.isValid()) {
            return;
        }

        const float z = m_transform.position.z + 0.00045f;
        const float rowCenterY =
            layout.frame.top + ((layout.plot.top - layout.frame.top) * 0.5f);
        const float chipHeight = layout.compact ? 12.f : 14.f;
        const float chipPadX = layout.compact ? 5.f : 6.f;
        const float chipGap = layout.compact ? 5.f : 6.f;
        const float swatchWidth = layout.compact ? 9.f : 10.f;
        float x = layout.plot.left;
        std::size_t traceIndex = 0;

        for (const auto &slotUuid : m_probedSlots) {
            const auto dataIt = m_probeData.find(slotUuid);
            const bool hidden = isProbeHidden(slotUuid);
            const std::string label =
                truncateLabel(slotLabelFor(*context.sceneState, slotUuid),
                              layout.compact ? 8 : 14);
            const std::string value =
                hidden ? "hidden"
                       : (dataIt != m_probeData.end()
                              ? latestVoltageLabel(dataIt->second)
                              : "--");
            const std::string legendText = std::format("{} {}", label, value);
            const auto textSize = context.renderer->measureText(
                legendText, {.fontSize = layout.labelFontSize});
            const float entryWidth =
                textSize.x + (chipPadX * 2.f) + swatchWidth + 7.f;

            if (x + entryWidth > layout.frame.right - 6.f ||
                traceIndex >= kLegendPickingInfoCount) {
                const std::size_t remaining = m_probedSlots.size() - traceIndex;
                const std::string moreLabel = std::format("+{}", remaining);
                const float moreOffsetY = context.renderer->textCenterOffsetY(
                    moreLabel, {.fontSize = layout.labelFontSize});
                drawText(context,
                         moreLabel,
                         {layout.frame.right - 20.f, rowCenterY + moreOffsetY},
                         z,
                         layout.labelFontSize,
                         ViewportTheme::sceneWidgetsColors.textMuted,
                         PickingId{m_runtimeId, 0});
                break;
            }

            const Color color = traceColorForIndex(traceIndex);
            const PickingId id{
                m_runtimeId,
                kLegendPickingInfoStart + static_cast<uint32_t>(traceIndex),
            };
            const Rect chipRect{
                .left = x,
                .top = rowCenterY - (chipHeight * 0.5f),
                .right = x + entryWidth,
                .bottom = rowCenterY + (chipHeight * 0.5f),
            };
            const Color chipFill =
                hidden
                    ? ViewportTheme::sceneWidgetsColors.surface.withAlpha(
                          ViewportTheme::isDark ? 0.36f : 0.48f)
                    : ViewportTheme::sceneWidgetsColors.surfaceHover.withAlpha(
                          ViewportTheme::isDark ? 0.62f : 0.74f);
            const Color chipHoverFill =
                hidden
                    ? ViewportTheme::sceneWidgetsColors.surfaceHover.withAlpha(
                          ViewportTheme::isDark ? 0.44f : 0.56f)
                    : ViewportTheme::sceneWidgetsColors.surfaceActive.withAlpha(
                          0.82f);
            const Color chipPressedFill =
                hidden
                    ? ViewportTheme::sceneWidgetsColors.surfaceActive.withAlpha(
                          ViewportTheme::isDark ? 0.52f : 0.64f)
                    : ViewportTheme::sceneWidgetsColors.surfaceActive;
            const Color chipBorder =
                hidden ? ViewportTheme::colors.componentBorder.withAlpha(0.34f)
                       : color.withAlpha(0.64f);
            const Color chipText =
                hidden ? ViewportTheme::sceneWidgetsColors.textMuted.withAlpha(
                             0.66f)
                       : ViewportTheme::colors.text;
            const Color swatchColor =
                hidden ? ViewportTheme::sceneWidgetsColors.textMuted.withAlpha(
                             0.38f)
                       : color.withAlpha(0.95f);
            SceneWidgets::ButtonOptions chipOptions{
                .textSize = layout.labelFontSize,
                .buttonSize = chipRect.size(),
                .padding = {0.f, 0.f},
                .borderThickness = glm::vec4(1.f),
                .borderRadius = glm::vec4(3.f),
                .backgroundColor = chipFill,
                .hoverBackgroundColor = chipHoverFill,
                .pressedBackgroundColor = chipPressedFill,
                .borderColor = chipBorder,
                .textColor = chipText,
            };
            if (SceneWidgets::button(
                    id,
                    "",
                    {chipRect.center().x, chipRect.center().y, z},
                    context,
                    chipOptions)) {
                auto before = toEditJson();
                toggleProbeVisibilityByLegendIndex(traceIndex);
                (void)Edit::trackComp(
                    *this, std::move(before), "monitor-legend");
            }

            const float swatchStartX = chipRect.left + chipPadX;
            const float swatchEndX = swatchStartX + swatchWidth;
            drawLine(context,
                     {swatchStartX, rowCenterY},
                     {swatchEndX, rowCenterY},
                     z + 0.0001f,
                     hidden ? 1.4f : 2.f,
                     swatchColor,
                     id);

            const float textOffsetY = context.renderer->textCenterOffsetY(
                legendText, {.fontSize = layout.labelFontSize});
            drawText(context,
                     legendText,
                     {swatchEndX + 7.f, rowCenterY + textOffsetY},
                     z + 0.0001f,
                     layout.labelFontSize,
                     chipText,
                     id);

            x += entryWidth + chipGap;
            ++traceIndex;
        }
    }

    void MonitorSceneComp::drawFollowLatestButton(SceneDrawContext &context,
                                                  const PlotLayout &layout) {
        if (!layout.outer.isValid()) {
            return;
        }

        constexpr float buttonWidth = 38.f;
        constexpr float buttonHeight = 14.f;
        const float z = m_transform.position.z + 0.00045f;
        const float padX = Styles::componentStyles.paddingX;
        const Rect buttonRect{
            .left = layout.outer.right - padX - buttonWidth,
            .top = layout.outer.top + 4.f,
            .right = layout.outer.right - padX,
            .bottom = layout.outer.top + 4.f + buttonHeight,
        };
        const Color buttonColor =
            m_followLatest ? ViewportTheme::sceneWidgetsColors.surfaceActive
                           : ViewportTheme::sceneWidgetsColors.surface;
        const Color borderColor =
            m_followLatest ? ViewportTheme::sceneWidgetsColors.borderFocus
                           : ViewportTheme::sceneWidgetsColors.border;
        const PickingId id{m_runtimeId, kFollowLatestButtonInfo};
        SceneWidgets::ButtonOptions buttonOptions{
            .textSize = layout.labelFontSize,
            .buttonSize = buttonRect.size(),
            .padding = {0.f, 0.f},
            .borderThickness = glm::vec4(1.f),
            .borderRadius = glm::vec4(3.f),
            .backgroundColor = buttonColor,
            .hoverBackgroundColor =
                ViewportTheme::sceneWidgetsColors.surfaceHover,
            .pressedBackgroundColor =
                ViewportTheme::sceneWidgetsColors.surfaceActive,
            .borderColor = borderColor,
            .textColor = ViewportTheme::colors.text,
        };
        if (SceneWidgets::button(
                id,
                "Live",
                {buttonRect.center().x, buttonRect.center().y, z},
                context,
                buttonOptions)) {
            auto before = toEditJson();
            resetPlotPan();
            (void)Edit::trackComp(*this, std::move(before), "monitor-view");
        }
    }

    void MonitorSceneComp::drawHoverReadout(SceneDrawContext &context,
                                            const PlotLayout &layout) const {
        if (!context.renderer || !context.sceneState || !m_isPlotHovered ||
            !layout.hasData || !layout.plot.isValid() ||
            layout.timeEndSec <= layout.timeStartSec ||
            layout.voltageMax <= layout.voltageMin) {
            return;
        }

        const glm::vec2 mousePos = context.sceneState->getMousePos();
        if (!pointInside(layout.plot, mousePos)) {
            return;
        }

        const float xNorm = std::clamp(
            (mousePos.x - layout.plot.left) / layout.plot.width(), 0.f, 1.f);
        const float yNorm = std::clamp(
            (layout.plot.bottom - mousePos.y) / layout.plot.height(), 0.f, 1.f);
        const double timeSec =
            layout.timeStartSec + ((layout.timeEndSec - layout.timeStartSec) *
                                   static_cast<double>(xNorm));
        const float voltage = layout.voltageMin +
                              ((layout.voltageMax - layout.voltageMin) * yNorm);

        const glm::vec2 readoutPoint{
            std::clamp(mousePos.x, layout.lineClip.left, layout.lineClip.right),
            std::clamp(mousePos.y, layout.lineClip.top, layout.lineClip.bottom),
        };
        const float z = m_transform.position.z + 0.0008f;
        const PickingId id{m_runtimeId, kPlotPickingInfo};
        const Color xAxisColor = ViewportTheme::colors.gridAxisXColor.withAlpha(
            ViewportTheme::isDark ? 0.76f : 0.84f);
        const Color yAxisColor = ViewportTheme::colors.gridAxisYColor.withAlpha(
            ViewportTheme::isDark ? 0.76f : 0.84f);
        const Color markerColor = yAxisColor.withAlpha(0.92f);

        drawLine(context,
                 {readoutPoint.x, layout.plot.top},
                 {readoutPoint.x, layout.plot.bottom},
                 z,
                 0.9f,
                 yAxisColor,
                 id);
        drawLine(context,
                 {layout.plot.left, readoutPoint.y},
                 {layout.plot.right, readoutPoint.y},
                 z,
                 0.9f,
                 xAxisColor,
                 id);

        Core::Renderer::CircleProps marker;
        marker.position = readoutPoint;
        marker.radius = layout.compact ? 2.f : 2.4f;
        marker.zIndex = z + 0.0001f;
        marker.color = markerColor;
        marker.id = id;
        marker.transformMode = context.transformMode;
        context.renderer->drawCircle(marker);

        Core::Renderer::CircleProps markerCore = marker;
        markerCore.radius = layout.compact ? 0.75f : 0.9f;
        markerCore.zIndex = z + 0.0002f;
        markerCore.color = ViewportTheme::colors.text.withAlpha(0.96f);
        context.renderer->drawCircle(markerCore);

        const float fontSize = layout.compact ? 6.f : 7.25f;
        const std::string xLabel =
            std::format("x  {}", formatTimeSeconds(timeSec));
        const std::string yLabel = std::format("y  {}", formatVoltage(voltage));
        const auto xSize =
            context.renderer->measureText(xLabel, {.fontSize = fontSize});
        const auto ySize =
            context.renderer->measureText(yLabel, {.fontSize = fontSize});
        const float padX = layout.compact ? 6.f : 7.f;
        const float padY = layout.compact ? 4.f : 5.f;
        const float lineHeight = std::max({xSize.y, ySize.y, fontSize}) + 2.f;
        const float tooltipWidth = std::max(xSize.x, ySize.x) + (padX * 2.f);
        const float tooltipHeight = (lineHeight * 2.f) + (padY * 2.f) - 2.f;
        if (tooltipWidth > layout.plot.width() - 8.f ||
            tooltipHeight > layout.plot.height() - 8.f) {
            return;
        }

        float left = readoutPoint.x + 8.f;
        float top = readoutPoint.y - tooltipHeight - 8.f;
        if (left + tooltipWidth > layout.plot.right - 4.f) {
            left = readoutPoint.x - tooltipWidth - 8.f;
        }
        if (top < layout.plot.top + 4.f) {
            top = readoutPoint.y + 8.f;
        }
        left = std::clamp(left,
                          layout.plot.left + 4.f,
                          layout.plot.right - tooltipWidth - 4.f);
        top = std::clamp(top,
                         layout.plot.top + 4.f,
                         layout.plot.bottom - tooltipHeight - 4.f);

        const Rect tooltipRect{
            .left = left,
            .top = top,
            .right = left + tooltipWidth,
            .bottom = top + tooltipHeight,
        };

        Core::Renderer::QuadProps tooltip;
        tooltip.position = tooltipRect.center();
        tooltip.size = tooltipRect.size();
        tooltip.zIndex = z + 0.00015f;
        tooltip.color =
            ViewportTheme::sceneWidgetsColors.popupSurface.withAlpha(
                ViewportTheme::isDark ? 0.94f : 0.97f);
        tooltip.id = id;
        tooltip.radius = glm::vec4(4.f);
        tooltip.borderColor = yAxisColor.withAlpha(0.72f);
        tooltip.thickness = glm::vec4(1.f);
        tooltip.shadow.enabled = true;
        tooltip.shadow.offset = {0.f, 3.f};
        tooltip.shadow.blur = 8.f;
        tooltip.shadow.spread = 0.5f;
        tooltip.shadow.color = Color{0.f, 0.f, 0.f, 0.28f};
        tooltip.transformMode = context.transformMode;
        context.renderer->drawQuad(tooltip);

        const float xTextY =
            tooltipRect.top + padY +
            context.renderer->textCenterOffsetY(xLabel, {.fontSize = fontSize});
        const float yTextY =
            tooltipRect.top + padY + lineHeight +
            context.renderer->textCenterOffsetY(yLabel, {.fontSize = fontSize});
        drawText(context,
                 xLabel,
                 {tooltipRect.left + padX, xTextY},
                 z + 0.00025f,
                 fontSize,
                 ViewportTheme::colors.text,
                 id);
        drawText(context,
                 yLabel,
                 {tooltipRect.left + padX, yTextY},
                 z + 0.00025f,
                 fontSize,
                 ViewportTheme::sceneWidgetsColors.textMuted,
                 id);
    }

    void MonitorSceneComp::drawEmptyState(SceneDrawContext &context,
                                          const PlotLayout &layout) const {
        if (!layout.plot.isValid()) {
            return;
        }

        const std::string label =
            m_probedSlots.empty()
                ? "No traces"
                : (visibleProbeCount() == 0 ? "All traces hidden"
                                            : "Waiting for samples");
        const auto textSize = context.renderer->measureText(
            label, {.fontSize = layout.labelFontSize});
        const float yOffset = context.renderer->textCenterOffsetY(
            label, {.fontSize = layout.labelFontSize});
        const glm::vec2 center = layout.plot.center();
        drawText(context,
                 label,
                 {center.x - (textSize.x * 0.5f), center.y + yOffset},
                 m_transform.position.z + 0.0004f,
                 layout.labelFontSize,
                 ViewportTheme::sceneWidgetsColors.textMuted,
                 PickingId{m_runtimeId, 2});
    }

    void MonitorSceneComp::plotProbedData(SceneDrawContext &context,
                                          const PlotLayout &layout) const {
        if (!layout.hasData || !layout.plot.isValid() ||
            layout.timeEndSec <= layout.timeStartSec ||
            layout.voltageMax <= layout.voltageMin) {
            return;
        }

        const double timeSpan = layout.timeEndSec - layout.timeStartSec;
        const float voltageSpan = layout.voltageMax - layout.voltageMin;
        const float z = m_transform.position.z + 0.00035f;
        float plotPixelWidth = layout.plot.width();
        if (context.transformMode ==
                Core::Renderer::RenderTransformMode::Camera &&
            context.camera) {
            plotPixelWidth *= context.camera->getZoom();
        }
        plotPixelWidth = std::max(plotPixelWidth, 1.f);

        const TimeNs viewStart = std::chrono::duration_cast<TimeNs>(
            std::chrono::duration<double>(layout.timeStartSec));
        const TimeNs viewEnd = std::chrono::duration_cast<TimeNs>(
            std::chrono::duration<double>(layout.timeEndSec));

        const auto pointFor = [&](double timeSec, float voltage) {
            const float xNorm =
                static_cast<float>((timeSec - layout.timeStartSec) / timeSpan);
            const float yNorm = (voltage - layout.voltageMin) / voltageSpan;
            return glm::vec2{
                layout.plot.left + (layout.plot.width() * xNorm),
                layout.plot.bottom - (layout.plot.height() * yNorm),
            };
        };

        std::size_t traceIndex = 0;
        for (const auto &slotUuid : m_probedSlots) {
            if (isProbeHidden(slotUuid)) {
                ++traceIndex;
                continue;
            }

            const auto dataIt = m_probeData.find(slotUuid);
            if (dataIt == m_probeData.end() || dataIt->second.empty()) {
                ++traceIndex;
                continue;
            }

            const auto &data = dataIt->second;
            const Color color = traceColorForIndex(traceIndex);
            const PickingId traceId{
                m_runtimeId,
                kTracePickingInfoStart +
                    static_cast<uint32_t>(
                        std::min<std::size_t>(traceIndex, 64)),
            };

            auto firstVisible =
                std::lower_bound(data.begin(),
                                 data.end(),
                                 viewStart,
                                 [](const auto &sample, TimeNs time) {
                                     return sample.first < time;
                                 });
            if (firstVisible != data.begin()) {
                --firstVisible;
            }
            const auto pastVisible =
                std::upper_bound(firstVisible,
                                 data.end(),
                                 viewEnd,
                                 [](TimeNs time, const auto &sample) {
                                     return time < sample.first;
                                 });
            if (firstVisible == pastVisible) {
                ++traceIndex;
                continue;
            }

            const std::size_t firstIndex = static_cast<std::size_t>(
                std::distance(data.begin(), firstVisible));
            const std::size_t pastIndex = static_cast<std::size_t>(
                std::distance(data.begin(), pastVisible));
            const std::size_t visibleSampleCount = pastIndex - firstIndex;

            std::vector<std::size_t> sampleIndices;
            const std::size_t directSampleLimit = static_cast<std::size_t>(
                std::max(2.f, std::ceil(plotPixelWidth * 2.f)));
            if (visibleSampleCount <= directSampleLimit) {
                sampleIndices.reserve(visibleSampleCount);
                for (std::size_t i = firstIndex; i < pastIndex; ++i) {
                    sampleIndices.push_back(i);
                }
            } else {
                const std::size_t bucketCount =
                    std::min(visibleSampleCount,
                             static_cast<std::size_t>(std::max(
                                 1.f, std::ceil(plotPixelWidth * 0.5f))));
                sampleIndices.reserve((bucketCount * 4U) + 2U);

                for (std::size_t bucket = 0; bucket < bucketCount; ++bucket) {
                    const std::size_t bucketBegin =
                        firstIndex +
                        ((visibleSampleCount * bucket) / bucketCount);
                    const std::size_t bucketEnd =
                        firstIndex +
                        ((visibleSampleCount * (bucket + 1U)) / bucketCount);
                    if (bucketBegin >= bucketEnd) {
                        continue;
                    }

                    std::size_t minIndex = bucketBegin;
                    std::size_t maxIndex = bucketBegin;
                    bool hasFiniteValue = false;
                    for (std::size_t i = bucketBegin; i < bucketEnd; ++i) {
                        const float value = data[i].second;
                        if (!std::isfinite(value)) {
                            continue;
                        }
                        if (!hasFiniteValue || value < data[minIndex].second) {
                            minIndex = i;
                        }
                        if (!hasFiniteValue || value > data[maxIndex].second) {
                            maxIndex = i;
                        }
                        hasFiniteValue = true;
                    }

                    std::array<std::size_t, 4> candidates{
                        bucketBegin,
                        hasFiniteValue ? minIndex : bucketBegin,
                        hasFiniteValue ? maxIndex : bucketBegin,
                        bucketEnd - 1U,
                    };
                    std::ranges::sort(candidates);
                    for (const std::size_t index : candidates) {
                        if (sampleIndices.empty() ||
                            sampleIndices.back() != index) {
                            sampleIndices.push_back(index);
                        }
                    }
                }
            }

            bool pathBegun = false;
            bool hasPathEnd = false;
            glm::vec2 pathEnd{0.f};
            const auto appendClippedSegment = [&](double timeA,
                                                  float voltageA,
                                                  double timeB,
                                                  float voltageB) {
                if (!std::isfinite(timeA) || !std::isfinite(timeB) ||
                    !std::isfinite(voltageA) || !std::isfinite(voltageB)) {
                    hasPathEnd = false;
                    return;
                }

                glm::vec2 start = pointFor(timeA, voltageA);
                glm::vec2 end = pointFor(timeB, voltageB);
                if (!clipLineToRect(start, end, layout.lineClip)) {
                    hasPathEnd = false;
                    return;
                }

                if (!pathBegun) {
                    context.renderer->beginPath({
                        .strokeColor = color,
                        .strokeSize = kTraceStroke,
                        .renderFill = false,
                        .zIndex = z,
                        .id = traceId,
                        .closePath = false,
                        .transformMode = context.transformMode,
                    });
                    pathBegun = true;
                }

                constexpr float samePointEpsilon = 1e-4f;
                if (!hasPathEnd ||
                    glm::length(pathEnd - start) > samePointEpsilon) {
                    context.renderer->pathMoveTo(start);
                }
                context.renderer->pathLineTo(end, kTraceStroke);
                pathEnd = end;
                hasPathEnd = true;
            };

            for (std::size_t i = 0; i + 1U < sampleIndices.size(); ++i) {
                const auto &[time, voltage] = data[sampleIndices[i]];
                const auto &[nextTime, nextVoltage] =
                    data[sampleIndices[i + 1U]];
                const double timeSec =
                    std::chrono::duration<double>(time).count();
                const double nextTimeSec =
                    std::chrono::duration<double>(nextTime).count();

                appendClippedSegment(timeSec, voltage, nextTimeSec, voltage);
                appendClippedSegment(
                    nextTimeSec, voltage, nextTimeSec, nextVoltage);
            }

            const auto &[lastTime, lastVoltage] = data[sampleIndices.back()];
            appendClippedSegment(
                std::chrono::duration<double>(lastTime).count(),
                lastVoltage,
                layout.timeEndSec,
                lastVoltage);
            if (pathBegun) {
                context.renderer->endPath();
            }

            constexpr float markerRadius = 2.3f;
            const glm::vec2 lastPoint =
                pointFor(layout.timeEndSec, lastVoltage);
            const Rect markerClip = inset(layout.plot,
                                          markerRadius,
                                          markerRadius,
                                          markerRadius,
                                          markerRadius);
            if (std::isfinite(lastVoltage) &&
                pointInside(markerClip, lastPoint)) {
                Bess::Core::Renderer::CircleProps marker;
                marker.position = lastPoint;
                marker.radius = markerRadius;
                marker.zIndex = z + 0.0001f;
                marker.color = color;
                marker.id = traceId;
                marker.transformMode = context.transformMode;
                context.renderer->drawCircle(marker);
            }
            ++traceIndex;
        }
    }

    bool MonitorSceneComp::onMouseEnter(const Events::MouseEnterEvent &e) {
        if (!isPlotPickingInfo(e.details)) {
            return false;
        }

        m_isPlotHovered = true;
        return true;
    }

    bool MonitorSceneComp::onMouseLeave(const Events::MouseLeaveEvent &e) {
        if (!isPlotPickingInfo(e.details)) {
            return false;
        }

        m_isPlotHovered = false;
        return true;
    }

    bool MonitorSceneComp::onMouseButton(const Events::MouseButtonEvent &e) {
        if (e.action == Events::MouseClickAction::press &&
            e.button == Events::MouseButton::left) {
            const auto &connStartSlot = e.sceneState->getConnectionStartSlot();
            if (connStartSlot != UUID::null) {
                const auto &comp =
                    e.sceneState->getComponentByUuid<SlotSceneComponent>(
                        connStartSlot);
                if (comp && comp->getType() == SceneComponentType::slot &&
                    !comp->isResizeSlot()) {
                    auto before = toEditJson();
                    addSlotProbe(*e.sceneState,
                                 e.sceneState->getConnectionStartSlot());
                    e.sceneState->setConnectionStartSlot(UUID::null);
                    (void)Edit::trackComp(
                        *this, std::move(before), "monitor-probe");
                    return true;
                }
            }
        }

        return false;
    }

    bool MonitorSceneComp::onMouseWheel(const Events::MouseWheelEvent &e) {
        if (!isPlotPickingInfo(e.details)) {
            return false;
        }

        auto before = toEditJson();
        m_timeScale =
            sanitizedScale(m_timeScale, 1.f, kMinTimeScale, kMaxTimeScale);
        const float zoomFactor = std::pow(1.15f, e.delta.y);
        if (!m_followLatest) {
            SceneDrawContext context;
            context.sceneState = e.sceneState;
            const PlotLayout layout = makePlotLayout(context);
            if (layout.hasData && layout.visibleTimeSpanSec > 0.0) {
                m_viewTimeSpanSeconds = std::max(
                    layout.visibleTimeSpanSec / zoomFactor, kMinTimeSpanSec);
            }
        }
        m_timeScale *= zoomFactor;
        m_timeScale =
            sanitizedScale(m_timeScale, 1.f, kMinTimeScale, kMaxTimeScale);
        (void)Edit::trackComp(*this, std::move(before), "monitor-view");
        return true;
    }

    void MonitorSceneComp::onMouseDragged(const Events::MouseDraggedEvent &e) {
        if (!isPlotPickingInfo(e.details)) {
            m_isPlotDragging = false;
            NonSimSceneComponent::onMouseDragged(e);
            return;
        }

        if (!m_isPlotDragging) {
            m_plotDragBefore = toEditJson();
            m_plotDragScene =
                e.sceneState ? e.sceneState->getSceneId() : UUID::null;
            m_isPlotDragging = true;
        }

        SceneDrawContext context;
        context.sceneState = e.sceneState;
        const PlotLayout layout = makePlotLayout(context);
        if (!layout.hasData || !layout.plot.isValid() ||
            layout.maxTimePanSec <= 0.0 || layout.visibleTimeSpanSec <= 0.0) {
            return;
        }

        const double secondsPerUnit =
            layout.visibleTimeSpanSec / layout.plot.width();
        if (m_followLatest || !std::isfinite(m_viewEndTimeSeconds)) {
            m_viewEndTimeSeconds = layout.timeEndSec;
        }
        if (!std::isfinite(m_viewTimeSpanSeconds) ||
            m_viewTimeSpanSeconds <= 0.0) {
            m_viewTimeSpanSeconds = layout.visibleTimeSpanSec;
        }
        m_followLatest = false;
        m_viewEndTimeSeconds -= static_cast<double>(e.delta.x) * secondsPerUnit;

        const double minEndSec = layout.maxTimeSec - layout.maxTimePanSec;
        m_viewEndTimeSeconds =
            std::clamp(m_viewEndTimeSeconds, minEndSec, layout.maxTimeSec);
    }

    void MonitorSceneComp::onMouseDragEnd() {
        if (m_isPlotDragging) {
            m_isPlotDragging = false;
            (void)Edit::trackComp(
                *this, std::move(m_plotDragBefore), "monitor-view");
            m_plotDragBefore = {};
            m_plotDragScene = UUID::null;
            return;
        }

        NonSimSceneComponent::onMouseDragEnd();
    }

    void MonitorSceneComp::drawPropertiesUI(SceneState &sceneState) {
        NonSimSceneComponent::drawPropertiesUI(sceneState);

        ImGui::Text("Probed Slots: %zu", m_probedSlots.size());

        float timeScale =
            sanitizedScale(m_timeScale, 1.f, kMinTimeScale, kMaxTimeScale);
        if (ImGui::DragFloat("Time Zoom",
                             &timeScale,
                             0.05f,
                             kMinTimeScale,
                             kMaxTimeScale,
                             "%.2fx")) {
            m_timeScale =
                sanitizedScale(timeScale, 1.f, kMinTimeScale, kMaxTimeScale);
        }

        float voltageScale = sanitizedScale(
            m_voltageScale, 1.f, kMinVoltageScale, kMaxVoltageScale);
        if (ImGui::DragFloat("Voltage Zoom",
                             &voltageScale,
                             0.05f,
                             kMinVoltageScale,
                             kMaxVoltageScale,
                             "%.2fx")) {
            m_voltageScale = sanitizedScale(
                voltageScale, 1.f, kMinVoltageScale, kMaxVoltageScale);
        }

        ImGui::Checkbox("Show Grid", &m_showGrid);
        ImGui::Checkbox("Show Legend", &m_showLegend);

        if (ImGui::Button("Reset Monitor View")) {
            m_timeScale = 1.f;
            m_voltageScale = 1.f;
            resetPlotPan();
            m_showGrid = true;
            m_showLegend = true;
        }
    }

    void MonitorSceneComp::resetPlotPan() {
        m_followLatest = true;
        m_viewEndTimeSeconds = 0.0;
        m_viewTimeSpanSeconds = 0.0;
        m_isPlotDragging = false;
    }

    bool MonitorSceneComp::isProbeHidden(const UUID &slotUuid) const {
        return m_hiddenProbedSlots.contains(slotUuid);
    }

    std::size_t MonitorSceneComp::visibleProbeCount() const {
        std::size_t count = 0;
        for (const auto &slotUuid : m_probedSlots) {
            if (!isProbeHidden(slotUuid)) {
                ++count;
            }
        }
        return count;
    }

    bool MonitorSceneComp::toggleProbeVisibilityByLegendIndex(
        std::size_t legendIndex) {
        std::size_t index = 0;
        for (const auto &slotUuid : m_probedSlots) {
            if (index == legendIndex) {
                if (isProbeHidden(slotUuid)) {
                    m_hiddenProbedSlots.erase(slotUuid);
                } else {
                    m_hiddenProbedSlots.insert(slotUuid);
                }
                return true;
            }
            ++index;
        }
        return false;
    }

    std::vector<UUID> MonitorSceneComp::cleanup(SceneState &state,
                                                UUID caller) {
        m_probeData.clear();
        m_probeSourceCursors.clear();
        m_probeBounds.clear();
        return NonSimSceneComponent::cleanup(state, caller);
    }

    void MonitorSceneComp::addSlotProbe(const SceneState &sceneState,
                                        const UUID &slotUuid) {
        (void)sceneState;
        if (m_probedSlots.contains(slotUuid)) {
            return;
        }

        m_probedSlots.insert(slotUuid);
        m_hiddenProbedSlots.erase(slotUuid);
    }

    void MonitorSceneComp::removeSlotProbe(const SceneState &sceneState,
                                           const UUID &slotUuid) {
        (void)sceneState;
        if (!m_probedSlots.contains(slotUuid)) {
            return;
        }

        m_probedSlots.erase(slotUuid);
        m_hiddenProbedSlots.erase(slotUuid);
        m_probeData.erase(slotUuid);
        m_probeSourceCursors.erase(slotUuid);
        m_probeBounds.erase(slotUuid);
    }

} // namespace Bess::Canvas
