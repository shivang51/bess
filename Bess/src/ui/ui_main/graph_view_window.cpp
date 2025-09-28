#include "ui/ui_main/graph_view_window.h"

#include "implot.h"
#include "scene/scene.h"
#include "ui/m_widgets.h"

namespace Bess::UI {

    GraphViewWindowData GraphViewWindow::s_data{};
    bool GraphViewWindow::isShown = false;

    LabeledDigitalSignal fetchSignal(const std::string &name, const Canvas::Components::SimulationComponent &comp) {
        const auto &data = SimEngine::SimulationEngine::instance().getStateMonitorData(comp.simEngineEntity);

        std::vector<std::pair<float, int>> parsedData;
        for (const auto &d : data) {
            parsedData.emplace_back(std::pair(d.first, (int)d.second));
        }
        return {name, parsedData};
    }

    void GraphViewWindow::draw() {
        if (!isShown)
            return;

        auto &reg = Canvas::Scene::instance().getEnttRegistry();
        const auto view = reg.view<Bess::Canvas::Components::TagComponent,
                             Bess::Canvas::Components::SimulationStateMonitor,
                             Bess::Canvas::Components::SimulationComponent>();

        ImGui::Begin(windowName.data(), nullptr, ImGuiWindowFlags_NoFocusOnAppearing);

        if (ImGui::Button("Add Graph")) {
            s_data.graphs[s_data.offset] = "";
            s_data.offset++;
        }

        std::vector<std::string> comps = {};
        std::unordered_map<std::string, entt::entity> entities = {};
        for (auto &ent : view) {
            const auto &tagComponent = view.get<Bess::Canvas::Components::TagComponent>(ent);
            comps.emplace_back(tagComponent.name);
            entities[tagComponent.name] = ent;
        }

        int eraseIdx = -1;
        for (auto &[idx, name] : s_data.graphs) {
            std::string prevName = name;
            if (MWidgets::ComboBox(std::format("Select node for graph {}", idx), name, comps)) {
                s_data.allSignals.erase(prevName);
            }

            if (name != "") {
                s_data.allSignals[name] = fetchSignal(name, view.get<Canvas::Components::SimulationComponent>(entities[name]));
            }

            ImGui::SameLine();

            if (ImGui::SmallButton(Icons::CodIcons::TRASH)) {
                eraseIdx = idx;
            }
        }

        if (eraseIdx != -1) {
            s_data.allSignals.erase(s_data.graphs[eraseIdx]);
            s_data.graphs.erase(eraseIdx);
        }

        plotDigitalSignals("Signals", s_data.allSignals);
        ImGui::End();
    }

    GraphViewWindowData &GraphViewWindow::getDataRef() {
        return s_data;
    }

    void GraphViewWindow::plotDigitalSignals(const std::string &plotName, const std::unordered_map<std::string, LabeledDigitalSignal> &signals, const float plotHeight) {
        if (signals.empty()) {
            return;
        }

        static double x_min = NAN, x_max = NAN;

        if (std::isnan(x_min) || std::isnan(x_max)) {
            x_min = 0.f;
            x_max = 100.f;
        }

        ImGui::BeginChild(plotName.c_str(), ImVec2(0, 0), false, ImGuiWindowFlags_None);

        const int numSignals = signals.size();

        int i = 0;
        for (auto &[name, signal] : signals) {
            const float height = plotHeight + (i == numSignals - 1 ? 20 : 0);

            if (ImPlot::BeginPlot(signal.name.c_str(), ImVec2(-1, height), ImPlotFlags_NoLegend | ImPlotFlags_NoTitle)) {
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
                const int dataCount = signal.values.size();

                const auto &data = signal.values;
                if (dataCount > 0) {
                    plotX.push_back(signal.values[0].first);
                    plotY.push_back(signal.values[0].second);
                }

                for (int i = 1; i < dataCount; ++i) {
                    if (data[i].second == plotY.back()) {
                        if (i == dataCount - 1) {
                            plotX.push_back(data[i].first);
                            plotY.push_back(data[i].second);
                        }
                        continue;
                    }

                    plotX.push_back(data[i].first);
                    plotY.push_back(data[i - 1].second);
                    plotX.push_back(data[i].first);
                    plotY.push_back(data[i].second);
                }

                ImPlot::PlotLine(name.c_str(), plotX.data(), plotY.data(), plotX.size());

                ImPlot::EndPlot();
            }
            i++;
        }

        ImGui::EndChild();
    }
} // namespace Bess::UI
