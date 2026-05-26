#pragma once
#include "bess_core/renderer/renderer_types.h"

namespace Bess::Core::Renderer {
    class IRenderer2D {
      public:
        virtual ~IRenderer2D();

        virtual void init() = 0;
        virtual void destroy() = 0;

        virtual void beginFrame() = 0;
        virtual void endFrame() = 0;

        virtual void clear(const Color &color) = 0;

        virtual void drawQuad(const QuadProps &props) = 0;

        virtual void
        drawRoundedQuad(const QuadProps &props,
                        const RoundedBorderProps &roundedProps) = 0;
    };

} // namespace Bess::Core::Renderer
