#pragma once

namespace Bess::Canvas::Styles {
    constexpr class CompNodeStyles {
      public:
        float headerHeight = 18.f;
        float headerFontSize = 10.f;
        float paddingX = 8.f;
        float paddingY = 4.f;
        float slotRadius = 4.5f;
        float slotMargin = 2.f;
        float slotBorderSize = 1.f;
        float rowMargin = 4.f;
        float rowGap = 4.f;
        float slotLabelSize = 10.f;

        float getSlotColumnSize() const {
            return (slotRadius * 2.f) + slotMargin + slotLabelSize;
        }
    } componentStyles;
} // namespace Bess::Canvas::Styles
