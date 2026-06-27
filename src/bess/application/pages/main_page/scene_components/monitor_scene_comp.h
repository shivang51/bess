#pragma once

#include "common/class_helpers.h"
#include "common/types.h"
#include "pages/main_page/scene_components/non_sim_scene_component.h"
#include <cstddef>

#define MONITOR_SCENE_COMP_SER_PROPS                                           \
    ("probedSlots", getProbedSlots, setProbedSlots),                           \
        ("hiddenProbedSlots", getHiddenProbedSlots, setHiddenProbedSlots),     \
        ("timeScale", getTimeScale, setTimeScale),                             \
        ("voltageScale", getVoltageScale, setVoltageScale),                    \
        ("followLatest", getFollowLatest, setFollowLatest),                    \
        ("viewEndTimeSeconds", getViewEndTimeSeconds, setViewEndTimeSeconds),  \
        ("viewTimeSpanSeconds",                                                \
         getViewTimeSpanSeconds,                                               \
         setViewTimeSpanSeconds),                                              \
        ("showGrid", getShowGrid, setShowGrid),                                \
        ("showLegend", getShowLegend, setShowLegend)

namespace Bess::Canvas {
    class MonitorSceneComp : public NonSimSceneComponent {
      public:
        MonitorSceneComp();
        ~MonitorSceneComp() override = default;

        REG_SCENE_COMP_TYPE("MonitorSceneComp",
                            SceneComponentType::nonSimulation)

        std::vector<std::shared_ptr<SceneComponent>>
        clone(const SceneState &sceneState) const override;

        void draw(SceneDrawContext &context) override;

        void update(TimeMs frameTime, SceneState &state) override;

        bool onMouseButton(const Events::MouseButtonEvent &e) override;
        bool onMouseWheel(const Events::MouseWheelEvent &e) override;
        bool onMouseEnter(const Events::MouseEnterEvent &e) override;
        bool onMouseLeave(const Events::MouseLeaveEvent &e) override;
        void onMouseDragged(const Events::MouseDraggedEvent &e) override;
        void onMouseDragEnd() override;

        void drawPropertiesUI(SceneState &sceneState) override;

        std::vector<UUID> cleanup(SceneState &state,
                                  UUID caller = UUID::null) override;

        void addSlotProbe(const SceneState &sceneState, const UUID &slotUuid);

        void removeSlotProbe(const SceneState &sceneState,
                             const UUID &slotUuid);

        MAKE_GETTER_SETTER(OrderedSet<UUID>, ProbedSlots, m_probedSlots)
        MAKE_GETTER_SETTER(OrderedSet<UUID>,
                           HiddenProbedSlots,
                           m_hiddenProbedSlots)
        MAKE_GETTER_SETTER(float, TimeScale, m_timeScale)
        MAKE_GETTER_SETTER(float, VoltageScale, m_voltageScale)
        MAKE_GETTER_SETTER(bool, FollowLatest, m_followLatest)
        MAKE_GETTER_SETTER(double, ViewEndTimeSeconds, m_viewEndTimeSeconds)
        MAKE_GETTER_SETTER(double, ViewTimeSpanSeconds, m_viewTimeSpanSeconds)
        MAKE_GETTER_SETTER(bool, ShowGrid, m_showGrid)
        MAKE_GETTER_SETTER(bool, ShowLegend, m_showLegend)

        SCENE_COMP_SER(MonitorSceneComp,
                       NonSimSceneComponent,
                       MONITOR_SCENE_COMP_SER_PROPS)

      private:
        struct PlotLayout;

        void subscribeToSlot(const SceneState &sceneState,
                             const UUID &slotUuid);
        void unsubscribeFromSlot(const SceneState &sceneState,
                                 const UUID &slotUuid);

        PlotLayout makePlotLayout(SceneDrawContext &context) const;
        void drawPlotFrame(SceneDrawContext &context,
                           const PlotLayout &layout) const;
        void drawGridAndLabels(SceneDrawContext &context,
                               const PlotLayout &layout) const;
        void drawLegend(SceneDrawContext &context, const PlotLayout &layout);
        void drawFollowLatestButton(SceneDrawContext &context,
                                    const PlotLayout &layout);
        void drawHoverReadout(SceneDrawContext &context,
                              const PlotLayout &layout) const;
        void drawEmptyState(SceneDrawContext &context,
                            const PlotLayout &layout) const;
        void plotProbedData(SceneDrawContext &context,
                            const PlotLayout &layout) const;
        void resetPlotPan();
        bool isProbeHidden(const UUID &slotUuid) const;
        std::size_t visibleProbeCount() const;
        bool toggleProbeVisibilityByLegendIndex(std::size_t legendIndex);

      private:
        OrderedSet<UUID> m_probedSlots;
        OrderedSet<UUID> m_hiddenProbedSlots;
        HashSet<UUID> m_subscribedSlots;
        HashMap<UUID, std::vector<std::pair<TimeNs, float>>> m_probeData;
        float m_timeScale = 1.f;
        float m_voltageScale = 1.f;
        bool m_followLatest = true;
        double m_viewEndTimeSeconds = 0.0;
        double m_viewTimeSpanSeconds = 0.0;
        bool m_isPlotHovered = false;
        bool m_isPlotDragging = false;
        bool m_showGrid = true;
        bool m_showLegend = true;
    };

} // namespace Bess::Canvas
