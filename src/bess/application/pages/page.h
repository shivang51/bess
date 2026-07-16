#pragma once

#include "common/bess_api.h"
#include "common/types.h"
#include <memory>

namespace Bess::Pages {
    class BESS_API Page : std::enable_shared_from_this<Page> {
      public:
        Page() = default;
        virtual ~Page() = default;

        virtual void draw() = 0;
        virtual void update(TimeMs ts) = 0;
    };
} // namespace Bess::Pages
