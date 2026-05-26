#pragma once

namespace Bess::Core::Renderer {
    class IRenderer {
      public:
        virtual ~IRenderer();

        virtual void init() = 0;
        virtual void destroy() = 0;

        virtual void beginFrame() = 0;
        virtual void endFrame() = 0;

        virtual void clear() = 0;
    };

} // namespace Bess::Core::Renderer
