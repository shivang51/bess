#pragma once

#include "common/bess_api.h"
#include "dock.h"
#include "ui_types.h"

#include <vector>

namespace Bess::UI {

    struct DockDropGuideMetrics {
        float indicatorSize = 38.f;
        float indicatorGap = 7.f;
        float previewInset = 5.f;
    };

    struct DockDropRegion {
        DockZone zone = DockZone::main;
        WidgetBounds indicatorBounds;
        WidgetBounds previewBounds;
    };

    // Geometry-only docking adorner description. The target node may be empty
    // when docking into an otherwise empty DockSpace; regions being non-empty
    // is therefore the validity signal.
    struct DockDropGuideLayout {
        DockNodeId target;
        WidgetBounds targetBounds;
        std::vector<DockDropRegion> regions;

        [[nodiscard]] const DockDropRegion *
        regionAt(glm::vec2 position) const noexcept;
        [[nodiscard]] const DockDropRegion *
        region(DockZone zone) const noexcept;
        [[nodiscard]] bool empty() const noexcept;
    };

    // Shared pure solver: rendering and pointer interaction consume the same
    // regions, keeping previews honest and making drag/drop testable without a
    // renderer.
    class BESS_API DockDropGuideLayoutSolver {
      public:
        [[nodiscard]] static DockDropGuideLayout
        calculate(WidgetBounds targetBounds,
                  DockNodeId target,
                  const DockDropGuideMetrics &metrics = {});

        [[nodiscard]] static WidgetBounds regionBounds(
            WidgetBounds bounds, DockZone zone, float inset = 0.f) noexcept;
    };

} // namespace Bess::UI
