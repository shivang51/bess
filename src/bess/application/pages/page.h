#pragma once
#include "common/types.h"
#include <memory>

namespace Bess::Pages {
    class Page : std::enable_shared_from_this<Page> {
      public:
        Page() = default;
        virtual ~Page() = default;

        virtual void draw() = 0;
        virtual void update(TimeMs ts) = 0;
    };
} // namespace Bess::Pages
