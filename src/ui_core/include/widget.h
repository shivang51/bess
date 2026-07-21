
#pragma once

#include "bess_core/renderer/renderer_2d.h"

namespace Bess::UI {

    struct UIDrawContext {
        std::shared_ptr<Core::Renderer::IRenderer2D> renderer = nullptr;
    };

    class Widget {
      public:
        MAKE_GETTER_SETTER(uint32_t, RuntimeId, m_runtimeId)

      protected:
        uint32_t m_runtimeId = 0;
    };
} // namespace Bess::UI
