#pragma once
#include "common/types.h"
#include "application/events/application_event.h"
#include <memory>
#include <vector>

namespace Bess::Pages {
    class Page : std::enable_shared_from_this<Page> {
      public:
        Page() = default;
        virtual ~Page() = default;

        virtual void draw() = 0;
        virtual void update(TimeMs ts, std::vector<ApplicationEvent> &events) = 0;
    };
} // namespace Bess::Pages
