#pragma once

#include "common/bess_api.h"
#include "common/sub_sys_container.h"

namespace Bess {
    class BESS_API GAppContext : public ISubSysContainer {
      public:
        static GAppContext &getInstance();

        GAppContext(const GAppContext &) = delete;
        GAppContext &operator=(const GAppContext &) = delete;

      private:
        GAppContext() = default;
    };
} // namespace Bess