#pragma once

#include "types.h"
namespace Bess {
    class ISubSystem {
      public:
        virtual ~ISubSystem() = default;

        // Called before any subsystem is intitalized
        virtual void onPreInit() {}

        virtual void onInit() = 0;

        // Called after all the subsystem have been intitalized
        virtual void onPostInit() {}

        // Called before all the subsystem are destroyed
        virtual void onShutdown() {}

        virtual void onDestroy() = 0;

        virtual void onUpdate(TimeMs dt) {}
    };
} // namespace Bess
