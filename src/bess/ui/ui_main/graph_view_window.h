#pragma once

#include "common/helpers.h"
#include "ui/icons/CodIcons.h"
#include "ui_panel.h"
#include <unordered_map>
#include <vector>

namespace Bess::UI {
    struct LabeledDigitalSignal {
        std::string name;
        std::vector<std::pair<float, int>> values;
    };

    struct GraphViewWindowData {
        int offset = 0;
        std::unordered_map<int, std::string> graphs;
        std::unordered_map<std::string, LabeledDigitalSignal> allSignals;
    };

    class GraphViewWindow : public Panel {
      public:
        GraphViewWindow();

        GraphViewWindowData &getDataRef();

        void onDraw() override;

      private:
        void plotDigitalSignals(const std::string &plotName,
                                const std::unordered_map<std::string, LabeledDigitalSignal> &signals,
                                float plotHeight = 150.0f);

      private:
        static GraphViewWindowData s_data;
    };
} // namespace Bess::UI
