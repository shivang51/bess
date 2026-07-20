
#pragma once

#include "bess_core/renderer/renderer_2d.h"

namespace Bess::UI {

    struct UIDrawContext {
        std::shared_ptr<Core::Renderer::IRenderer2D> renderer = nullptr;
    };

    class Widget {};
} // namespace Bess::UI
