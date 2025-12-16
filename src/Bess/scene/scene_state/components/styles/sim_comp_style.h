#pragma once

#include "scene/scene_state/components/styles/comp_style.h"

namespace Bess::Canvas::Styles {
    constexpr class SimCompNodeStyles : public CompNodeStyles {
    } simCompStyles;

    constexpr float SIM_COMP_SLOT_DX = simCompStyles.paddingX +
                                       simCompStyles.slotRadius +
                                       simCompStyles.slotMargin;
    constexpr float SIM_COMP_SLOT_START_Y = simCompStyles.headerHeight;
    constexpr float SIM_COMP_SLOT_ROW_SIZE = (simCompStyles.rowMargin * 2.f) +
                                             (simCompStyles.slotRadius * 2.f) +
                                             simCompStyles.rowGap;
    constexpr float SIM_COMP_SLOT_COLUMN_SIZE = (simCompStyles.slotRadius +
                                                 simCompStyles.slotMargin +
                                                 simCompStyles.slotLabelSize) *
                                                2;

} // namespace Bess::Canvas::Styles
