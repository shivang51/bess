#pragma once

#include "common/helpers.h"
#include "ui/icons/CodIcons.h"

namespace Bess::UI {
    struct LabeledDigitalSignal {
        std::string name;
        std::vector<std::pair<float, int>> values;
    };

    struct GraphViewWindowData {
        int offset = 0;
        std::unordered_map<int, std::string> graphs = {};
        std::unordered_map<std::string, LabeledDigitalSignal> allSignals = {};
    };

    class GraphViewWindow {
      public:
        static void hide();
        static void show();
        static void draw();

        static bool isShown;

        static constexpr auto windowName = Common::Helpers::concat(Icons::CodIcons::GRAPH_LINE, "  Graph View");

        static GraphViewWindowData &getDataRef();

      private:
        static void plotDigitalSignals(const std::string &plotName, const std::unordered_map<std::string, LabeledDigitalSignal> &signals, float plotHeight = 150.0f);

      private:
        static GraphViewWindowData s_data;
    };
} // namespace Bess::UI
