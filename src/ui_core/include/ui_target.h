#pragma once

#include "bess_core/renderer/texture.h"
#include "common/types.h"
#include "dock.h"
#include "layout.h"
#include <memory>

namespace Bess::UI {

    struct UITargetInpCtx {
        PickingId pickingId = PickingId::invalid();
        glm::vec2 mousePos = {0, 0};
    };

    struct Rect {
        glm::vec2 pos = {0, 0};
        glm::vec2 size = {0, 0};
    };

    // This will be a root UIContainer
    // It will have:
    // - its own textures for drawing and picking
    // - a UINodes (layout engine nodes) container
    // - a dock manager
    // - and ui widgets
    class UITarget {
      public:
        UITarget() = default;
        ~UITarget() = default;

        MAKE_GETTER_SETTER(std::shared_ptr<Core::Renderer::ITexture>,
                           DrawTexture,
                           m_drawTex);

        MAKE_GETTER_SETTER(std::shared_ptr<Core::Renderer::ITexture>,
                           PickingTexture,
                           m_pickingTex);

      private:
        DockManager m_dockManager;
        LayoutNodeRegistry m_layoutNodesReg;
        std::shared_ptr<Core::Renderer::ITexture> m_drawTex = nullptr;
        std::shared_ptr<Core::Renderer::ITexture> m_pickingTex = nullptr;
        Rect m_rect{};
        UITargetInpCtx m_inpCtx;
    };
} // namespace Bess::UI
