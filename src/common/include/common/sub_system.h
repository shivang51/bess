#pragma once

#include "common/bess_api.h"
#include "types.h"
namespace Bess {
    class BESS_API ISubSystem {
      public:
        virtual ~ISubSystem() = default;

        virtual void onBeginFrame() {}
        virtual void onEndFrame() {}

        // Called before any subsystem is intitalized
        virtual void onPreInit() {}

        virtual void onInit() = 0;

        // Called after all the subsystem have been intitalized
        virtual void onPostInit() {}

        // Called before all the subsystem are destroyed
        virtual void onShutdown() {}

        virtual void onDestroy() = 0;

        virtual void onPreUpdate() {}
        virtual void onUpdate(TimeMs dt) {}

        virtual void onPreDraw() {}
        virtual void onDraw() {}
        virtual void onPostDraw() {}
    };
} // namespace Bess
