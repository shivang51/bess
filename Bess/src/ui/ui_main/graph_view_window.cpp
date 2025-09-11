#include "ui/ui_main/graph_view_window.h"

#include "scene/scene.h"

#include "ui/m_widgets.h"

namespace Bess::UI {

    GraphViewWindowData GraphViewWindow::s_data{};

    void GraphViewWindow::hide() {
        s_data.isWindowShown = false;
    }

    void GraphViewWindow::show() {
        s_data.isWindowShown = true;
    }

    void GraphViewWindow::draw() {
        if (!s_data.isWindowShown)
            return;

        auto &reg = Canvas::Scene::instance().getEnttRegistry();
        auto view = reg.view<Bess::Canvas::Components::TagComponent,
                             Bess::Canvas::Components::SimulationStateMonitor,
                             Bess::Canvas::Components::SimulationComponent>();

        ImGui::Begin(windowName.data(), nullptr, ImGuiWindowFlags_NoFocusOnAppearing);

        std::vector<std::string> comps = {};
        std::unordered_map<std::string, entt::entity> entities = {};
        for (auto &ent : view) {
            const auto &tagComponent = view.get<Bess::Canvas::Components::TagComponent>(ent);
            comps.emplace_back(tagComponent.name);
            entities[tagComponent.name] = ent;
        }

        static std::string selected = "";
        MWidgets::ComboBox("Select a node", selected, comps);

        // if (selected != "") {
        //     allSignals[selected] = fetchSignal(selected, view.get<Canvas::Components::SimulationComponent>(entities[selected]));
        // }
        //
        // PlotDigitalSignals("Signals", allSignals);

        ImGui::End();
    }

    GraphViewWindowData &GraphViewWindow::getDataRef() {
        return s_data;
    }
} // namespace Bess::UI
