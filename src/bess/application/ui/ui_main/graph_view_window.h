#pragma once

#include "common/bess_api.h"

#include "ui/ui_panel.h"
#include <unordered_map>
#include <vector>

namespace Bess::UI {
    struct BESS_API LabeledDigitalSignal {
        std::string name;
        std::vector<std::pair<float, int>> values;
    };

    struct BESS_API GraphViewWindowData {
        int offset = 0;
        std::unordered_map<int, std::pair<std::string, UUID>> graphs;
        std::unordered_map<UUID, LabeledDigitalSignal> allSignals;
    };

    class BESS_API GraphViewWindow : public Panel {
      public:
        GraphViewWindow();

        GraphViewWindowData &getDataRef();

        void onDraw() override;
        void destroy() override;

      private:
        void plotDigitalSignals(
            const std::string &plotName,
            const std::unordered_map<UUID, LabeledDigitalSignal> &signals,
            float plotHeight = 150.0f);

      private:
        static GraphViewWindowData s_data;
    };
} // namespace Bess::UI
