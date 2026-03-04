#include "ui/ui_main/graph_view_window.h"

#include "common/bess_uuid.h"
#include "common/helpers.h"
#include "implot.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/scene_components/slot_probe_scene_component.h"
#include "types.h"
#include "ui/icons/CodIcons.h"
#include "ui/widgets/m_widgets.h"

namespace Bess::UI {

    GraphViewWindowData GraphViewWindow::s_data{};
    static constexpr auto windowName = Common::Helpers::concat(Icons::CodIcons::GRAPH_LINE, "  Graph View");
    GraphViewWindow::GraphViewWindow() : Panel(std::string(windowName.data())) {
        m_visible = true;
    }

    LabeledDigitalSignal fetchSignal(const std::string &name, const UUID &probeId) {
        const auto &scene = Pages::MainPage::getInstance()->getState().getSceneDriver().getActiveScene();
        const auto &sceneState = scene->getState();
        const auto &probeComp = sceneState.getComponentByUuid<Canvas::SlotProbeSceneComponent>(probeId);
        BESS_ASSERT(probeComp,
                    std::format("Probe component with uuid {} not found in scene state", (uint64_t)probeId));

        std::vector<std::pair<float, int>> parsedData;
        for (const auto &d : probeComp->getProbeData()) {
            auto timeSec = std::chrono::duration<float>(d.first).count();
            parsedData.emplace_back(timeSec,
                                    d.second == SimEngine::LogicState::high ? 1 : 0);
        }
        return {probeComp->getName(), parsedData};
    }

    void GraphViewWindow::onDraw() {
        const auto &mainPage = Pages::MainPage::getInstance();
        const auto &mainPageState = mainPage->getState();
        const auto &sceneState = mainPageState.getSceneDriver()->getState();

        if (ImGui::Button("Add Graph")) {
            s_data.graphs[s_data.offset].first = "";
            s_data.graphs[s_data.offset].second = UUID::null;
            s_data.offset++;
        }

        std::vector<std::string> comps = {};
        std::vector<UUID> compIds = {};
        std::unordered_map<std::string, UUID> nameToIdMap;
        for (const auto &id : mainPageState.getProbes()) {
            const auto &comp = sceneState.getComponentByUuid<Canvas::SlotProbeSceneComponent>(id);
            comps.emplace_back(comp->getName());
            compIds.emplace_back(id);
            nameToIdMap[comp->getName()] = id;
        }

        int eraseIdx = -1;
        for (auto &[idx, val] : s_data.graphs) {
            const auto &prevId = val.second;
            if (Widgets::ComboBox(std::format("Select node for graph {}", idx), val.first, comps)) {
                s_data.allSignals.erase(prevId);
                val.second = nameToIdMap.at(val.first);
            }

            if (val.first != "") {
                s_data.allSignals[val.second] = fetchSignal(val.first, val.second);
            }

            ImGui::SameLine();

            if (ImGui::SmallButton(Icons::CodIcons::TRASH)) {
                eraseIdx = idx;
            }
        }

        if (eraseIdx != -1) {
            s_data.allSignals.erase(s_data.graphs[eraseIdx].second);
            s_data.graphs.erase(eraseIdx);
        }

        plotDigitalSignals("Signals", s_data.allSignals);
    }

    GraphViewWindowData &GraphViewWindow::getDataRef() {
        return s_data;
    }

    void GraphViewWindow::plotDigitalSignals(const std::string &plotName,
                                             const std::unordered_map<UUID, LabeledDigitalSignal> &signals,
                                             const float plotHeight) {
        if (signals.empty()) {
            return;
        }

        static double x_min = NAN, x_max = NAN;

        if (std::isnan(x_min) || std::isnan(x_max)) {
            x_min = 0.f;
            x_max = 100.f;
        }

        ImGui::BeginChild(plotName.c_str(), ImVec2(0, 0), false, ImGuiWindowFlags_None);

        const auto numSignals = signals.size();

        int i = 0;
        for (auto &[name, signal] : signals) {
            const float height = plotHeight + (i == numSignals - 1 ? 20.f : 0.f);

            if (ImPlot::BeginPlot(signal.name.c_str(),
                                  ImVec2(-1, height),
                                  ImPlotFlags_NoLegend | ImPlotFlags_NoTitle)) {
                ImPlot::SetupAxis(ImAxis_Y1, signal.name.c_str(), ImPlotAxisFlags_NoTickLabels);
                ImPlot::SetupAxisLimits(ImAxis_Y1, -0.2, 1.2, ImGuiCond_Always);

                ImPlot::SetupAxisLinks(ImAxis_X1, &x_min, &x_max);

                if (i < numSignals - 1) {
                    ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoTickLabels);
                } else {
                    ImPlot::SetupAxis(ImAxis_X1, "Time");
                }

                std::vector<double> plotX;
                std::vector<double> plotY;
                const auto dataCount = signal.values.size();

                const auto &data = signal.values;
                if (dataCount > 0) {
                    plotX.push_back(signal.values[0].first);
                    plotY.push_back(signal.values[0].second);
                }

                for (int i = 1; i < dataCount; ++i) {
                    plotX.push_back(data[i].first);
                    plotY.push_back(data[i - 1].second);
                    plotX.push_back(data[i].first);
                    plotY.push_back(data[i].second);
                }

                ImPlot::PlotLine(name.toString().c_str(), plotX.data(), plotY.data(), (int)plotX.size());

                ImPlot::EndPlot();
            }
            i++;
        }

        ImGui::EndChild();
    }

    void GraphViewWindow::destroy() {
        s_data = {};
    }
} // namespace Bess::UI
