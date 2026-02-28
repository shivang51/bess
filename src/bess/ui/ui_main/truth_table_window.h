#pragma once

#include "common/bess_uuid.h"
#include "types.h"
#include "ui_panel.h"
#include <string>

namespace Bess::UI {
    class TruthTableWindow : public Panel {
      public:
        TruthTableWindow();
        void onDraw() override;

      private:
        std::string *selectedNetName;
        UUID selectedNetId;
        bool isDirty;
        SimEngine::TruthTable currentTruthTable;
        std::unordered_map<UUID, std::string> compIdToNameMap;
    };
} // namespace Bess::UI
