#pragma once

#include "common/bess_uuid.h"
#include "ui/ui_panel.h"

#include <string>

namespace Bess::UI {
    class BESS_API StampDataViewer : public Panel {
      public:
        StampDataViewer();

      private:
        void onDraw() override;

        UUID m_selectedComponentId = UUID::null;
        std::string m_componentFilter;
        std::string m_statusMessage;
        int m_timeUnit = 0;
        bool m_newestFirst = true;
        bool m_statusIsError = false;
    };
} // namespace Bess::UI
