#include "controls/dock_drop.h"

#include <algorithm>
#include <cmath>

namespace Bess::UI {
    namespace {
        bool finiteVec(glm::vec2 value) noexcept {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }
    } // namespace

    const DockDropRegion *
    DockDropGuideLayout::regionAt(glm::vec2 position) const noexcept {
        const auto it = std::find_if(
            regions.begin(), regions.end(), [position](const auto &entry) {
                return entry.indicatorBounds.contains(position);
            });
        return it != regions.end() ? &*it : nullptr;
    }

    const DockDropRegion *
    DockDropGuideLayout::region(DockZone zone) const noexcept {
        const auto it = std::find_if(
            regions.begin(), regions.end(), [zone](const auto &entry) {
                return entry.zone == zone;
            });
        return it != regions.end() ? &*it : nullptr;
    }

    bool DockDropGuideLayout::empty() const noexcept {
        return regions.empty();
    }

    WidgetBounds DockDropGuideLayoutSolver::regionBounds(WidgetBounds bounds,
                                                         DockZone zone,
                                                         float inset) noexcept {
        bounds = bounds.inset(std::max(0.f, inset));
        if (bounds.empty() || zone == DockZone::main) {
            return bounds;
        }

        const auto topLeft = bounds.topLeft();
        if (zone == DockZone::left || zone == DockZone::right) {
            const float width = bounds.size.x * 0.5f;
            return {
                .center = {zone == DockZone::left ? topLeft.x + width * 0.5f
                                                  : topLeft.x + width * 1.5f,
                           bounds.center.y},
                .size = {width, bounds.size.y},
            };
        }

        const float height = bounds.size.y * 0.5f;
        return {
            .center = {bounds.center.x,
                       zone == DockZone::top ? topLeft.y + height * 0.5f
                                             : topLeft.y + height * 1.5f},
            .size = {bounds.size.x, height},
        };
    }

    DockDropGuideLayout
    DockDropGuideLayoutSolver::calculate(WidgetBounds targetBounds,
                                         DockNodeId target,
                                         const DockDropGuideMetrics &metrics) {
        DockDropGuideLayout result{
            .target = target,
            .targetBounds = targetBounds,
        };
        if (targetBounds.empty() || !finiteVec(targetBounds.center) ||
            !finiteVec(targetBounds.size)) {
            return result;
        }

        const float shortest =
            std::min(targetBounds.size.x, targetBounds.size.y);
        const float gap =
            std::min(std::max(0.f, metrics.indicatorGap), shortest / 16.f);
        const float size =
            std::min(std::max(0.f, metrics.indicatorSize),
                     std::max(0.f, (shortest - gap * 2.f) / 3.f));
        if (size < 8.f) {
            return result;
        }

        const float offset = size + gap;
        const auto indicator = [size](glm::vec2 center) {
            return WidgetBounds{.center = center, .size = {size, size}};
        };
        const auto add = [&](DockZone zone, glm::vec2 center) {
            result.regions.push_back({
                .zone = zone,
                .indicatorBounds = indicator(center),
                .previewBounds =
                    regionBounds(targetBounds, zone, metrics.previewInset),
            });
        };

        add(DockZone::main, targetBounds.center);
        add(DockZone::left, targetBounds.center + glm::vec2{-offset, 0.f});
        add(DockZone::right, targetBounds.center + glm::vec2{offset, 0.f});
        add(DockZone::top, targetBounds.center + glm::vec2{0.f, -offset});
        add(DockZone::bottom, targetBounds.center + glm::vec2{0.f, offset});
        return result;
    }

} // namespace Bess::UI
